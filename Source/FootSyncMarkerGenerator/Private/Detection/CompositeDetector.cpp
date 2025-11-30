// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#include "Detection/CompositeDetector.h"
#include "FootSyncMarkerSettings.h"

FCompositeDetector::FCompositeDetector()
{
	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();
	PelvisCrossingWeight = Settings->CompositeWeights.PelvisCrossingWeight;
	VelocityCurveWeight = Settings->CompositeWeights.VelocityCurveWeight;
	SaliencyWeight = Settings->CompositeWeights.SaliencyWeight;

	PelvisDetector = MakeUnique<FPelvisCrossingDetector>();
	VelocityDetector = MakeUnique<FVelocityCurveDetector>();
	SaliencyDetector = MakeUnique<FSaliencyDetector>();
}

FCompositeDetector::FCompositeDetector(const FCompositeDetectionWeights& Weights)
	: PelvisCrossingWeight(Weights.PelvisCrossingWeight)
	, VelocityCurveWeight(Weights.VelocityCurveWeight)
	, SaliencyWeight(Weights.SaliencyWeight)
{
	PelvisDetector = MakeUnique<FPelvisCrossingDetector>();
	VelocityDetector = MakeUnique<FVelocityCurveDetector>();
	SaliencyDetector = MakeUnique<FSaliencyDetector>();
}

TArray<FFootContactResult> FCompositeDetector::DetectContacts(
	UAnimSequence* AnimSequence,
	const FSyncFootDefinition& Foot,
	const FLocomotionPreset& Preset)
{
	// Run all detectors
	TArray<FFootContactResult> PelvisResults;
	TArray<FFootContactResult> VelocityResults;
	TArray<FFootContactResult> SaliencyResults;

	if (PelvisDetector && PelvisCrossingWeight > KINDA_SMALL_NUMBER)
	{
		PelvisResults = PelvisDetector->DetectContacts(AnimSequence, Foot, Preset);
	}

	if (VelocityDetector && VelocityCurveWeight > KINDA_SMALL_NUMBER)
	{
		VelocityResults = VelocityDetector->DetectContacts(AnimSequence, Foot, Preset);
	}

	if (SaliencyDetector && SaliencyWeight > KINDA_SMALL_NUMBER)
	{
		SaliencyResults = SaliencyDetector->DetectContacts(AnimSequence, Foot, Preset);
	}

	UE_LOG(LogAnimation, Verbose,
		TEXT("CompositeDetector: Pelvis=%d, Velocity=%d, Saliency=%d results"),
		PelvisResults.Num(), VelocityResults.Num(), SaliencyResults.Num());

	// Merge results
	return MergeResults(PelvisResults, VelocityResults, SaliencyResults);
}

TArray<FFootContactResult> FCompositeDetector::MergeResults(
	const TArray<FFootContactResult>& PelvisResults,
	const TArray<FFootContactResult>& VelocityResults,
	const TArray<FFootContactResult>& SaliencyResults)
{
	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();

	// Combine all results
	TArray<FFootContactResult> AllResults;
	AllResults.Append(PelvisResults);
	AllResults.Append(VelocityResults);
	AllResults.Append(SaliencyResults);

	if (AllResults.Num() == 0)
	{
		return TArray<FFootContactResult>();
	}

	// Sort by time
	AllResults.Sort([](const FFootContactResult& A, const FFootContactResult& B)
	{
		return A.Time < B.Time;
	});

	// Cluster by time proximity
	TArray<TArray<FFootContactResult>> Clusters = ClusterResultsByTime(
		AllResults, Settings->ResultMergeThreshold);

	// Calculate final results from clusters
	TArray<FFootContactResult> FinalResults;
	for (const TArray<FFootContactResult>& Cluster : Clusters)
	{
		if (Cluster.Num() > 0)
		{
			FinalResults.Add(CalculateClusterResult(Cluster));
		}
	}

	return FinalResults;
}

TArray<TArray<FFootContactResult>> FCompositeDetector::ClusterResultsByTime(
	const TArray<FFootContactResult>& AllResults,
	float MergeThreshold)
{
	TArray<TArray<FFootContactResult>> Clusters;

	if (AllResults.Num() == 0)
	{
		return Clusters;
	}

	// Start first cluster
	TArray<FFootContactResult> CurrentCluster;
	CurrentCluster.Add(AllResults[0]);
	float ClusterStartTime = AllResults[0].Time;

	for (int32 i = 1; i < AllResults.Num(); ++i)
	{
		const FFootContactResult& Result = AllResults[i];

		// Check if this result is close enough to the current cluster
		if (Result.Time - ClusterStartTime <= MergeThreshold)
		{
			CurrentCluster.Add(Result);
		}
		else
		{
			// Save current cluster and start new one
			Clusters.Add(CurrentCluster);
			CurrentCluster.Empty();
			CurrentCluster.Add(Result);
			ClusterStartTime = Result.Time;
		}
	}

	// Don't forget the last cluster
	if (CurrentCluster.Num() > 0)
	{
		Clusters.Add(CurrentCluster);
	}

	return Clusters;
}

FFootContactResult FCompositeDetector::CalculateClusterResult(
	const TArray<FFootContactResult>& Cluster)
{
	if (Cluster.Num() == 0)
	{
		return FFootContactResult();
	}

	if (Cluster.Num() == 1)
	{
		return Cluster[0];
	}

	// Calculate weighted average time and confidence
	float WeightedTimeSum = 0.0f;
	float TotalWeight = 0.0f;
	float MaxConfidence = 0.0f;
	int32 ContactVotes = 0;
	int32 LiftOffVotes = 0;

	for (const FFootContactResult& Result : Cluster)
	{
		float MethodWeight = GetWeightForMethod(Result.Source);
		float CombinedWeight = MethodWeight * Result.Confidence;

		WeightedTimeSum += Result.Time * CombinedWeight;
		TotalWeight += CombinedWeight;
		MaxConfidence = FMath::Max(MaxConfidence, Result.Confidence);

		// Vote on contact vs lift-off
		if (Result.bIsContact)
		{
			ContactVotes++;
		}
		else
		{
			LiftOffVotes++;
		}
	}

	FFootContactResult MergedResult;

	// Time is weighted average
	if (TotalWeight > KINDA_SMALL_NUMBER)
	{
		MergedResult.Time = WeightedTimeSum / TotalWeight;
	}
	else
	{
		// Fallback to simple average
		float Sum = 0.0f;
		for (const FFootContactResult& Result : Cluster)
		{
			Sum += Result.Time;
		}
		MergedResult.Time = Sum / Cluster.Num();
	}

	// Confidence is higher when multiple detectors agree
	// Base confidence + bonus for agreement
	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();
	float AgreementBonus = (Cluster.Num() - 1) * Settings->DetectorAgreementBonus;
	MergedResult.Confidence = FMath::Clamp(MaxConfidence + AgreementBonus, 0.0f, 1.0f);

	// Majority vote for contact/lift-off
	MergedResult.bIsContact = ContactVotes >= LiftOffVotes;

	// Source is composite
	MergedResult.Source = EFootContactDetectionMethod::Composite;

	return MergedResult;
}

float FCompositeDetector::GetWeightForMethod(EFootContactDetectionMethod Method) const
{
	switch (Method)
	{
	case EFootContactDetectionMethod::PelvisCrossing:
		return PelvisCrossingWeight;
	case EFootContactDetectionMethod::VelocityCurve:
		return VelocityCurveWeight;
	case EFootContactDetectionMethod::Saliency:
		return SaliencyWeight;
	default:
		return 1.0f;
	}
}

void FCompositeDetector::SetVelocityThreshold(float Threshold)
{
	if (VelocityDetector)
	{
		VelocityDetector->SetVelocityThreshold(Threshold);
	}
}

void FCompositeDetector::SetSaliencyThreshold(float Threshold)
{
	if (SaliencyDetector)
	{
		SaliencyDetector->SetSaliencyThreshold(Threshold);
	}
}

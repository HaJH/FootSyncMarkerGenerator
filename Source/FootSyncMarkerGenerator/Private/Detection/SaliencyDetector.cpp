// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#include "Detection/SaliencyDetector.h"
#include "FootSyncMarkerSettings.h"
#include "AnimPose.h"
#include "AnimationBlueprintLibrary.h"
#include "Animation/AnimSequence.h"

TArray<FFootContactResult> FSaliencyDetector::DetectContacts(
	UAnimSequence* AnimSequence,
	const FSyncFootDefinition& Foot,
	const FLocomotionPreset& Preset)
{
	TArray<FFootContactResult> Results;

	if (!AnimSequence || Foot.BoneName.IsNone())
	{
		return Results;
	}

	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();

	// Get animation info
	float SequenceLength;
	UAnimationBlueprintLibrary::GetSequenceLength(AnimSequence, SequenceLength);

	int32 NumKeys;
	UAnimationBlueprintLibrary::GetNumKeys(AnimSequence, NumKeys);

	if (NumKeys < 4)  // Need at least 4 frames for curvature analysis
	{
		return Results;
	}

	// Build time intervals
	TArray<float> Times;
	TArray<double> TimeIntervals;
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		float Time;
		UAnimationBlueprintLibrary::GetTimeAtFrame(AnimSequence, KeyIndex, Time);
		Times.Add(Time);
		TimeIntervals.Add(static_cast<double>(Time));
	}

	// Evaluate all poses
	TArray<FAnimPose> Poses;
	FAnimPoseEvaluationOptions Options;
	Options.EvaluationType = EAnimDataEvalType::Source;

	UAnimPoseExtensions::GetAnimPoseAtTimeIntervals(AnimSequence, TimeIntervals, Options, Poses);

	if (Poses.Num() != Times.Num())
	{
		UE_LOG(LogAnimation, Warning,
			TEXT("SaliencyDetector: Pose count mismatch"));
		return Results;
	}

	// Get foot positions in world space
	TArray<FVector> Positions;
	for (const FAnimPose& Pose : Poses)
	{
		FTransform FootTransform = UAnimPoseExtensions::GetBonePose(
			Pose, Foot.BoneName, EAnimPoseSpaces::World);
		Positions.Add(FootTransform.GetLocation());
	}

	// Calculate curvature at each point
	TArray<float> Curvatures = CalculateCurvature(Positions, Times);

	if (Curvatures.Num() < 3)
	{
		return Results;
	}

	// Find salient points
	const float Threshold = bHasThresholdOverride ? SaliencyThresholdOverride : Settings->SaliencyThreshold;
	TArray<int32> SalientIndices = FindSalientPoints(
		Curvatures, Times,
		Settings->SaliencyWindowSize,
		Threshold);

	// Calculate max curvature for confidence scaling
	float MaxCurvature = 0.0f;
	for (float Curv : Curvatures)
	{
		MaxCurvature = FMath::Max(MaxCurvature, Curv);
	}

	// Convert salient points to results
	for (int32 Index : SalientIndices)
	{
		if (Index >= 0 && Index < Times.Num() && Index < Curvatures.Num())
		{
			float Curvature = Curvatures[Index];

			// Confidence based on curvature prominence
			float Confidence = MaxCurvature > KINDA_SMALL_NUMBER
				? FMath::Clamp(Curvature / MaxCurvature, Settings->SaliencyMinConfidence, 1.0f)
				: Settings->SaliencyDefaultConfidence;

			// Determine if this is a contact or lift-off
			bool bIsContact = IsFootContact(Positions, Index);

			Results.Add(FFootContactResult(
				Times[Index],
				Confidence,
				bIsContact,
				EFootContactDetectionMethod::Saliency
			));
		}
	}

	return Results;
}

TArray<float> FSaliencyDetector::CalculateCurvature(
	const TArray<FVector>& Positions,
	const TArray<float>& Times)
{
	TArray<float> Curvatures;

	if (Positions.Num() < 3)
	{
		return Curvatures;
	}

	// First point has no curvature (need neighbors)
	Curvatures.Add(0.0f);

	// Calculate curvature for interior points
	for (int32 i = 1; i < Positions.Num() - 1; ++i)
	{
		float Curvature = CalculatePointCurvature(
			Positions[i - 1], Positions[i], Positions[i + 1]);
		Curvatures.Add(Curvature);
	}

	// Last point has no curvature
	Curvatures.Add(0.0f);

	return Curvatures;
}

float FSaliencyDetector::CalculatePointCurvature(
	const FVector& P0, const FVector& P1, const FVector& P2)
{
	// Menger curvature formula:
	// k = 4 * Area(triangle) / (|P0-P1| * |P1-P2| * |P2-P0|)
	// Area = 0.5 * |cross(P1-P0, P2-P0)|

	FVector V1 = P1 - P0;
	FVector V2 = P2 - P0;
	FVector V3 = P2 - P1;

	float A = V1.Size();
	float B = V3.Size();
	float C = V2.Size();

	// Cross product for area calculation
	FVector CrossProduct = FVector::CrossProduct(V1, V2);
	float TriangleAreaTimesTwo = CrossProduct.Size();

	// Avoid division by zero
	float Denominator = A * B * C;
	if (Denominator < KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	// Curvature = 4 * (Area * 2 / 2) / (A * B * C) = 2 * |cross| / (A * B * C)
	float Curvature = (2.0f * TriangleAreaTimesTwo) / Denominator;

	return Curvature;
}

TArray<int32> FSaliencyDetector::FindSalientPoints(
	const TArray<float>& Curvatures,
	const TArray<float>& Times,
	float WindowSize,
	float Threshold)
{
	TArray<int32> SalientIndices;

	if (Curvatures.Num() < 3 || Times.Num() != Curvatures.Num())
	{
		return SalientIndices;
	}

	// Calculate curvature derivative (rate of change)
	TArray<float> CurvatureDerivatives;
	CurvatureDerivatives.Add(0.0f);  // First point

	for (int32 i = 1; i < Curvatures.Num(); ++i)
	{
		float DeltaTime = Times[i] - Times[i - 1];
		if (DeltaTime > KINDA_SMALL_NUMBER)
		{
			float Derivative = FMath::Abs(Curvatures[i] - Curvatures[i - 1]) / DeltaTime;
			CurvatureDerivatives.Add(Derivative);
		}
		else
		{
			CurvatureDerivatives.Add(0.0f);
		}
	}

	// Calculate statistics for adaptive thresholding
	float MeanDerivative = 0.0f;
	float MaxDerivative = 0.0f;
	for (float Deriv : CurvatureDerivatives)
	{
		MeanDerivative += Deriv;
		MaxDerivative = FMath::Max(MaxDerivative, Deriv);
	}
	MeanDerivative /= CurvatureDerivatives.Num();

	// Adaptive threshold based on the data
	float AdaptiveThreshold = MeanDerivative + Threshold * (MaxDerivative - MeanDerivative);

	// Find points where curvature derivative exceeds threshold
	// and are local maxima of curvature
	for (int32 i = 1; i < Curvatures.Num() - 1; ++i)
	{
		// Check if this is a curvature peak
		bool bIsCurvaturePeak = Curvatures[i] > Curvatures[i - 1] &&
								Curvatures[i] > Curvatures[i + 1];

		// Check if curvature derivative is high (rapid change)
		bool bHighDerivative = CurvatureDerivatives[i] > AdaptiveThreshold ||
							   (i + 1 < CurvatureDerivatives.Num() &&
								CurvatureDerivatives[i + 1] > AdaptiveThreshold);

		// Also check for inflection points (sign change in second derivative of position)
		// which correspond to changes in movement direction

		if (bIsCurvaturePeak || bHighDerivative)
		{
			// Check if this is sufficiently far from other salient points
			bool bTooClose = false;
			for (int32 ExistingIndex : SalientIndices)
			{
				if (FMath::Abs(Times[i] - Times[ExistingIndex]) < WindowSize)
				{
					bTooClose = true;
					break;
				}
			}

			if (!bTooClose)
			{
				SalientIndices.Add(i);
			}
		}
	}

	return SalientIndices;
}

bool FSaliencyDetector::IsFootContact(
	const TArray<FVector>& Positions,
	int32 SalientIndex)
{
	// Look at the height (Z) change around the salient point
	// If height is decreasing before and increasing after, it's a contact
	// If height is increasing before and decreasing after, it's a lift-off

	if (SalientIndex <= 0 || SalientIndex >= Positions.Num() - 1)
	{
		return true;  // Default to contact
	}

	// Look at a small window around the salient point
	int32 WindowStart = FMath::Max(0, SalientIndex - 2);
	int32 WindowEnd = FMath::Min(Positions.Num() - 1, SalientIndex + 2);

	float HeightBefore = 0.0f;
	int32 CountBefore = 0;
	for (int32 i = WindowStart; i < SalientIndex; ++i)
	{
		HeightBefore += Positions[i].Z;
		CountBefore++;
	}
	if (CountBefore > 0) HeightBefore /= CountBefore;

	float HeightAfter = 0.0f;
	int32 CountAfter = 0;
	for (int32 i = SalientIndex + 1; i <= WindowEnd; ++i)
	{
		HeightAfter += Positions[i].Z;
		CountAfter++;
	}
	if (CountAfter > 0) HeightAfter /= CountAfter;

	float HeightAtPoint = Positions[SalientIndex].Z;

	// Contact: height was decreasing (above), now at minimum or increasing
	// Lift-off: height was stable or at minimum, now increasing
	bool bWasDecreasing = HeightBefore > HeightAtPoint;
	bool bWillIncrease = HeightAfter > HeightAtPoint;

	// If height was going down and will go up (or stay), it's a contact
	// This is a simplification - more sophisticated analysis could consider velocity
	return bWasDecreasing || !bWillIncrease;
}

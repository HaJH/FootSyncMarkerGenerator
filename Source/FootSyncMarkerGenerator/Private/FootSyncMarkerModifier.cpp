// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#include "FootSyncMarkerModifier.h"
#include "FootSyncMarkerSettings.h"
#include "Detection/IFootContactDetector.h"
#include "Detection/PelvisCrossingDetector.h"
#include "Detection/VelocityCurveDetector.h"
#include "Detection/SaliencyDetector.h"
#include "Detection/CompositeDetector.h"
#include "AnimationBlueprintLibrary.h"
#include "AnimPose.h"
#include "Animation/AnimSequence.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/CoreStyle.h"

UFootSyncMarkerModifier::UFootSyncMarkerModifier()
{
}

void UFootSyncMarkerModifier::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
	if (!AnimationSequence)
	{
		UE_LOG(LogAnimation, Warning, TEXT("FootSyncMarkerModifier: Invalid animation sequence"));
		return;
	}

	// Get effective preset
	FLocomotionPreset Preset = GetEffectivePreset(AnimationSequence);

	if (!Preset.IsValid())
	{
		UE_LOG(LogAnimation, Warning,
			TEXT("FootSyncMarkerModifier: Could not create valid preset for %s. "
				 "Check bone patterns in settings or use Custom preset."),
			*AnimationSequence->GetName());
		return;
	}

	UE_LOG(LogAnimation, Log,
		TEXT("FootSyncMarkerModifier: Processing %s with %d feet"),
		*AnimationSequence->GetName(), Preset.Feet.Num());

	ProcessAnimation(AnimationSequence, Preset);
}

void UFootSyncMarkerModifier::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
	if (!AnimationSequence)
	{
		return;
	}

	FLocomotionPreset Preset = GetEffectivePreset(AnimationSequence);
	RemoveGeneratedData(AnimationSequence, Preset);

	UE_LOG(LogAnimation, Log,
		TEXT("FootSyncMarkerModifier: Reverted %s"),
		*AnimationSequence->GetName());
}

void UFootSyncMarkerModifier::ProcessAnimation(
	UAnimSequence* AnimSequence, const FLocomotionPreset& Preset)
{
	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();

	// Ensure the sync marker track exists
	if (!UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(AnimSequence, Settings->SyncMarkerTrackName))
	{
		UAnimationBlueprintLibrary::AddAnimationNotifyTrack(
			AnimSequence, Settings->SyncMarkerTrackName, FLinearColor::Green);
	}

	// Process each foot
	for (const FSyncFootDefinition& Foot : Preset.Feet)
	{
		if (Foot.BoneName.IsNone())
		{
			UE_LOG(LogAnimation, Warning,
				TEXT("FootSyncMarkerModifier: Skipping foot with empty bone name"));
			continue;
		}

		// Detect foot contacts
		TArray<FFootContactResult> Results = DetectFootContacts(AnimSequence, Foot, Preset);

		// Filter contact points only
		TArray<FFootContactResult> ContactResults;
		for (const FFootContactResult& Result : Results)
		{
			if (Result.bIsContact)
			{
				ContactResults.Add(Result);
			}
		}

		// Sort by confidence (descending)
		ContactResults.Sort([](const FFootContactResult& A, const FFootContactResult& B)
		{
			return A.Confidence > B.Confidence;
		});

		// Select top N by confidence (if MaxMarkersPerFoot > 0)
		const int32 MaxMarkers = GetEffectiveMaxMarkersPerFoot();
		if (MaxMarkers > 0 && ContactResults.Num() > MaxMarkers)
		{
			ContactResults.SetNum(MaxMarkers);
		}

		// Filter by confidence threshold
		const float MinConfidence = GetEffectiveMinimumConfidence();
		TArray<FFootContactResult> ConfidentResults;
		for (const FFootContactResult& Result : ContactResults)
		{
			if (Result.Confidence > MinConfidence)
			{
				ConfidentResults.Add(Result);
			}
		}

		// Guarantee minimum one if enabled
		if (ConfidentResults.Num() == 0 && GetEffectiveGuaranteeMinimumOne() && ContactResults.Num() > 0)
		{
			ConfidentResults.Add(ContactResults[0]);  // Best confidence one

			// Show notification instead of blocking dialog
			FNotificationInfo Info(FText::Format(
				NSLOCTEXT("FootSyncMarker", "LowConfidenceWarning",
					"Foot {0}: No contacts above threshold, using best confidence ({1})"),
				FText::FromName(Foot.BoneName),
				FText::AsNumber(ContactResults[0].Confidence)));
			Info.ExpireDuration = 3.0f;
			Info.bUseSuccessFailIcons = true;
			Info.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.WarningWithColor"));
			FSlateNotificationManager::Get().AddNotification(Info);
		}

		// Sort by time for interval filtering
		ConfidentResults.Sort([](const FFootContactResult& A, const FFootContactResult& B)
		{
			return A.Time < B.Time;
		});

		// Remove duplicates within minimum interval
		TArray<float> FilteredTimes;
		for (const FFootContactResult& Result : ConfidentResults)
		{
			if (FilteredTimes.Num() == 0 ||
				(Result.Time - FilteredTimes.Last()) > Settings->MinimumMarkerInterval)
			{
				FilteredTimes.Add(Result.Time);
			}
		}

		UE_LOG(LogAnimation, Log,
			TEXT("  Foot %s: detected %d contacts, confident %d, filtered to %d markers"),
			*Foot.BoneName.ToString(), ContactResults.Num(), ConfidentResults.Num(), FilteredTimes.Num());

		// Add sync markers
		AddSyncMarkers(AnimSequence, Foot, FilteredTimes);

		// Generate curves if enabled
		if (bGenerateDistanceCurves || bGenerateVelocityCurves)
		{
			GenerateCurves(AnimSequence, Foot, Preset);
		}
	}
}

TArray<FFootContactResult> UFootSyncMarkerModifier::DetectFootContacts(
	UAnimSequence* AnimSequence,
	const FSyncFootDefinition& Foot,
	const FLocomotionPreset& Preset)
{
	EFootContactDetectionMethod Method = GetEffectiveDetectionMethod();

	TUniquePtr<IFootContactDetector> Detector = CreateDetector(Method);
	if (Detector)
	{
		// Apply per-animation threshold overrides
		if (bOverrideVelocityThreshold)
		{
			Detector->SetVelocityThreshold(VelocityThresholdOverride);
		}
		if (bOverrideSaliencyThreshold)
		{
			Detector->SetSaliencyThreshold(SaliencyThresholdOverride);
		}

		return Detector->DetectContacts(AnimSequence, Foot, Preset);
	}

	UE_LOG(LogAnimation, Warning,
		TEXT("FootSyncMarkerModifier: Failed to create detector for method %d"),
		static_cast<int32>(Method));

	return TArray<FFootContactResult>();
}

void UFootSyncMarkerModifier::AddSyncMarkers(
	UAnimSequence* AnimSequence,
	const FSyncFootDefinition& Foot,
	const TArray<float>& ContactTimes)
{
	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();

	for (float Time : ContactTimes)
	{
		UAnimationBlueprintLibrary::AddAnimationSyncMarker(
			AnimSequence,
			Foot.MarkerName,
			Time,
			Settings->SyncMarkerTrackName
		);
	}
}

void UFootSyncMarkerModifier::GenerateCurves(
	UAnimSequence* AnimSequence,
	const FSyncFootDefinition& Foot,
	const FLocomotionPreset& Preset)
{
	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();

	// Get animation length and frame count
	float SequenceLength;
	UAnimationBlueprintLibrary::GetSequenceLength(AnimSequence, SequenceLength);

	int32 NumKeys;
	UAnimationBlueprintLibrary::GetNumKeys(AnimSequence, NumKeys);

	if (NumKeys <= 1)
	{
		return;
	}

	// Evaluate poses at each frame
	TArray<double> TimeIntervals;
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		float Time;
		UAnimationBlueprintLibrary::GetTimeAtFrame(AnimSequence, KeyIndex, Time);
		TimeIntervals.Add(static_cast<double>(Time));
	}

	TArray<FAnimPose> Poses;
	FAnimPoseEvaluationOptions Options;
	Options.EvaluationType = EAnimDataEvalType::Source;

	UAnimPoseExtensions::GetAnimPoseAtTimeIntervals(AnimSequence, TimeIntervals, Options, Poses);

	// Calculate distances and velocities
	TArray<float> Times;
	TArray<float> Distances;
	TArray<float> Velocities;

	FVector PrevPosition = FVector::ZeroVector;
	float PrevTime = 0.0f;

	for (int32 i = 0; i < Poses.Num(); ++i)
	{
		float CurrentTime = static_cast<float>(TimeIntervals[i]);
		Times.Add(CurrentTime);

		// Get relative transform from pelvis to foot
		FTransform RelativeTransform = UAnimPoseExtensions::GetRelativeTransform(
			Poses[i], Preset.PelvisBoneName, Foot.BoneName, EAnimPoseSpaces::World);

		FVector CurrentPosition = RelativeTransform.GetLocation();

		// Distance from pelvis
		float Distance = CurrentPosition | Preset.PrimaryMoveAxis;
		Distances.Add(Distance);

		// Velocity (from frame 1 onward)
		if (i > 0)
		{
			float DeltaTime = CurrentTime - PrevTime;
			if (DeltaTime > KINDA_SMALL_NUMBER)
			{
				float Velocity = (CurrentPosition - PrevPosition).Size() / DeltaTime;
				Velocities.Add(Velocity);
			}
			else
			{
				Velocities.Add(0.0f);
			}
		}
		else
		{
			Velocities.Add(0.0f);
		}

		PrevPosition = CurrentPosition;
		PrevTime = CurrentTime;
	}

	// Get foot label string for curve naming
	FString FootLabel;
	switch (Foot.FootLabel)
	{
	case EFootLabel::Left:			FootLabel = TEXT("Left"); break;
	case EFootLabel::Right:			FootLabel = TEXT("Right"); break;
	case EFootLabel::FrontLeft:		FootLabel = TEXT("FrontLeft"); break;
	case EFootLabel::FrontRight:	FootLabel = TEXT("FrontRight"); break;
	case EFootLabel::BackLeft:		FootLabel = TEXT("BackLeft"); break;
	case EFootLabel::BackRight:		FootLabel = TEXT("BackRight"); break;
	case EFootLabel::Custom:		FootLabel = Foot.CustomLabel; break;
	}

	// Generate distance curve
	if (bGenerateDistanceCurves)
	{
		FName DistanceCurveName = FName(*(FootLabel + Settings->DistanceCurveSuffix));

		// Remove existing curve if present
		if (UAnimationBlueprintLibrary::DoesCurveExist(
				AnimSequence, DistanceCurveName, ERawCurveTrackTypes::RCT_Float))
		{
			UAnimationBlueprintLibrary::RemoveCurve(AnimSequence, DistanceCurveName, false);
		}

		// Add new curve
		UAnimationBlueprintLibrary::AddCurve(
			AnimSequence, DistanceCurveName, ERawCurveTrackTypes::RCT_Float, false);
		UAnimationBlueprintLibrary::AddFloatCurveKeys(
			AnimSequence, DistanceCurveName, Times, Distances);
	}

	// Generate velocity curve
	if (bGenerateVelocityCurves)
	{
		FName VelocityCurveName = FName(*(FootLabel + Settings->VelocityCurveSuffix));

		// Remove existing curve if present
		if (UAnimationBlueprintLibrary::DoesCurveExist(
				AnimSequence, VelocityCurveName, ERawCurveTrackTypes::RCT_Float))
		{
			UAnimationBlueprintLibrary::RemoveCurve(AnimSequence, VelocityCurveName, false);
		}

		// Add new curve
		UAnimationBlueprintLibrary::AddCurve(
			AnimSequence, VelocityCurveName, ERawCurveTrackTypes::RCT_Float, false);
		UAnimationBlueprintLibrary::AddFloatCurveKeys(
			AnimSequence, VelocityCurveName, Times, Velocities);
	}
}

void UFootSyncMarkerModifier::RemoveGeneratedData(
	UAnimSequence* AnimSequence, const FLocomotionPreset& Preset)
{
	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();

	// Remove sync markers
	UAnimationBlueprintLibrary::RemoveAnimationSyncMarkersByTrack(
		AnimSequence, Settings->SyncMarkerTrackName);

	// Remove curves for each foot
	for (const FSyncFootDefinition& Foot : Preset.Feet)
	{
		FString FootLabel;
		switch (Foot.FootLabel)
		{
		case EFootLabel::Left:			FootLabel = TEXT("Left"); break;
		case EFootLabel::Right:			FootLabel = TEXT("Right"); break;
		case EFootLabel::FrontLeft:		FootLabel = TEXT("FrontLeft"); break;
		case EFootLabel::FrontRight:	FootLabel = TEXT("FrontRight"); break;
		case EFootLabel::BackLeft:		FootLabel = TEXT("BackLeft"); break;
		case EFootLabel::BackRight:		FootLabel = TEXT("BackRight"); break;
		case EFootLabel::Custom:		FootLabel = Foot.CustomLabel; break;
		}

		// Remove distance curve
		FName DistanceCurveName = FName(*(FootLabel + Settings->DistanceCurveSuffix));
		if (UAnimationBlueprintLibrary::DoesCurveExist(
				AnimSequence, DistanceCurveName, ERawCurveTrackTypes::RCT_Float))
		{
			UAnimationBlueprintLibrary::RemoveCurve(AnimSequence, DistanceCurveName, false);
		}

		// Remove velocity curve
		FName VelocityCurveName = FName(*(FootLabel + Settings->VelocityCurveSuffix));
		if (UAnimationBlueprintLibrary::DoesCurveExist(
				AnimSequence, VelocityCurveName, ERawCurveTrackTypes::RCT_Float))
		{
			UAnimationBlueprintLibrary::RemoveCurve(AnimSequence, VelocityCurveName, false);
		}
	}
}

TUniquePtr<IFootContactDetector> UFootSyncMarkerModifier::CreateDetector(
	EFootContactDetectionMethod Method)
{
	switch (Method)
	{
	case EFootContactDetectionMethod::PelvisCrossing:
		return MakeUnique<FPelvisCrossingDetector>();

	case EFootContactDetectionMethod::VelocityCurve:
		return MakeUnique<FVelocityCurveDetector>();

	case EFootContactDetectionMethod::Saliency:
		return MakeUnique<FSaliencyDetector>();

	case EFootContactDetectionMethod::Composite:
		return MakeUnique<FCompositeDetector>();

	default:
		UE_LOG(LogAnimation, Warning,
			TEXT("FootSyncMarkerModifier: Unknown detection method %d, using Composite"),
			static_cast<int32>(Method));
		return MakeUnique<FCompositeDetector>();
	}
}

FLocomotionPreset UFootSyncMarkerModifier::GetEffectivePreset(UAnimSequence* AnimSequence) const
{
	if (LocomotionType == ELocomotionType::Custom)
	{
		return CustomPreset;
	}

	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();
	const USkeleton* Skeleton = AnimSequence->GetSkeleton();

	return Settings->CreatePresetForSkeleton(Skeleton, LocomotionType);
}

EFootContactDetectionMethod UFootSyncMarkerModifier::GetEffectiveDetectionMethod() const
{
	if (bOverrideDetectionMethod)
	{
		return DetectionMethodOverride;
	}

	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();
	return Settings->DetectionMethod;
}

float UFootSyncMarkerModifier::GetEffectiveMinimumConfidence() const
{
	if (bOverrideMinimumConfidence)
	{
		return MinimumConfidenceOverride;
	}

	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();
	return Settings->MinimumConfidence;
}

float UFootSyncMarkerModifier::GetEffectiveVelocityThreshold() const
{
	if (bOverrideVelocityThreshold)
	{
		return VelocityThresholdOverride;
	}

	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();
	return Settings->VelocityMinimumThreshold;
}

float UFootSyncMarkerModifier::GetEffectiveSaliencyThreshold() const
{
	if (bOverrideSaliencyThreshold)
	{
		return SaliencyThresholdOverride;
	}

	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();
	return Settings->SaliencyThreshold;
}

int32 UFootSyncMarkerModifier::GetEffectiveMaxMarkersPerFoot() const
{
	if (bOverrideMaxMarkersPerFoot)
	{
		return MaxMarkersPerFootOverride;
	}

	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();
	return Settings->MaxMarkersPerFoot;
}

bool UFootSyncMarkerModifier::GetEffectiveGuaranteeMinimumOne() const
{
	if (bOverrideGuaranteeMinimumOne)
	{
		return bGuaranteeMinimumOneOverride;
	}

	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();
	return Settings->bGuaranteeMinimumOne;
}

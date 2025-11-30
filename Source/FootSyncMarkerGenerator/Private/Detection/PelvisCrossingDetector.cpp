// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#include "Detection/PelvisCrossingDetector.h"
#include "FootSyncMarkerSettings.h"
#include "AnimPose.h"
#include "AnimationBlueprintLibrary.h"
#include "Animation/AnimSequence.h"

TArray<FFootContactResult> FPelvisCrossingDetector::DetectContacts(
	UAnimSequence* AnimSequence,
	const FSyncFootDefinition& Foot,
	const FLocomotionPreset& Preset)
{
	TArray<FFootContactResult> Results;

	if (!AnimSequence || Foot.BoneName.IsNone() || Preset.PelvisBoneName.IsNone())
	{
		return Results;
	}

	const UFootSyncMarkerSettings* Settings = UFootSyncMarkerSettings::Get();

	// Get animation info
	float SequenceLength;
	UAnimationBlueprintLibrary::GetSequenceLength(AnimSequence, SequenceLength);

	int32 NumKeys;
	UAnimationBlueprintLibrary::GetNumKeys(AnimSequence, NumKeys);

	if (NumKeys < 2)
	{
		return Results;
	}

	// Build time intervals for batch evaluation
	TArray<double> TimeIntervals;
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		float Time;
		UAnimationBlueprintLibrary::GetTimeAtFrame(AnimSequence, KeyIndex, Time);
		TimeIntervals.Add(static_cast<double>(Time));
	}

	// Evaluate all poses at once for performance
	TArray<FAnimPose> Poses;
	FAnimPoseEvaluationOptions Options;
	Options.EvaluationType = EAnimDataEvalType::Source;

	UAnimPoseExtensions::GetAnimPoseAtTimeIntervals(AnimSequence, TimeIntervals, Options, Poses);

	if (Poses.Num() != TimeIntervals.Num())
	{
		UE_LOG(LogAnimation, Warning,
			TEXT("PelvisCrossingDetector: Pose count mismatch (%d vs %d)"),
			Poses.Num(), TimeIntervals.Num());
		return Results;
	}

	// Track positions along the move axis
	TArray<float> Positions;
	for (int32 i = 0; i < Poses.Num(); ++i)
	{
		float Position = GetFootPositionAlongAxis(
			Poses[i], Preset.PelvisBoneName, Foot.BoneName, Preset.PrimaryMoveAxis);
		Positions.Add(Position);
	}

	// Find zero crossings (pelvis line crossings)
	for (int32 i = 1; i < Positions.Num(); ++i)
	{
		float PrevPos = Positions[i - 1];
		float CurrPos = Positions[i];

		// Check for sign change (crossing the pelvis line)
		if (PrevPos * CurrPos < 0.0f)
		{
			float PrevTime = static_cast<float>(TimeIntervals[i - 1]);
			float CurrTime = static_cast<float>(TimeIntervals[i]);

			// Interpolate the exact crossing time
			float CrossingTime = InterpolateCrossingTime(PrevTime, PrevPos, CurrTime, CurrPos);

			// Determine if this is a foot contact (foot moving backward to forward)
			// or foot lift-off (foot moving forward to backward)
			bool bIsContact = (PrevPos < 0.0f && CurrPos > 0.0f);

			// Calculate confidence based on the magnitude of position change
			float PositionChange = FMath::Abs(CurrPos - PrevPos);
			float Confidence = FMath::Clamp(PositionChange / Settings->PelvisConfidenceScale, 0.5f, 1.0f);

			Results.Add(FFootContactResult(
				CrossingTime,
				Confidence,
				bIsContact,
				EFootContactDetectionMethod::PelvisCrossing
			));
		}
	}

	// Check for crossing at the loop boundary (for looping animations)
	if (Positions.Num() >= 2)
	{
		float FirstPos = Positions[0];
		float LastPos = Positions.Last();

		if (FirstPos * LastPos < 0.0f)
		{
			// There's a crossing between the last and first frame
			// For looping animations, add a marker near the end or start
			float LastTime = static_cast<float>(TimeIntervals.Last());
			float FirstTime = static_cast<float>(TimeIntervals[0]);

			// Use the end time as an approximation
			bool bIsContact = (LastPos < 0.0f && FirstPos > 0.0f);

			Results.Add(FFootContactResult(
				LastTime,  // or could use FirstTime for start
				Settings->LoopBoundaryConfidence,
				bIsContact,
				EFootContactDetectionMethod::PelvisCrossing
			));
		}
	}

	return Results;
}

float FPelvisCrossingDetector::GetFootPositionAlongAxis(
	const FAnimPose& Pose,
	FName PelvisBone,
	FName FootBone,
	const FVector& MoveAxis)
{
	// Get relative transform from pelvis to foot in world space
	FTransform RelativeTransform = UAnimPoseExtensions::GetRelativeTransform(
		Pose, PelvisBone, FootBone, EAnimPoseSpaces::World);

	// Project the relative position onto the move axis
	FVector RelativePosition = RelativeTransform.GetLocation();
	float PositionAlongAxis = FVector::DotProduct(RelativePosition, MoveAxis);

	return PositionAlongAxis;
}

float FPelvisCrossingDetector::InterpolateCrossingTime(
	float Time1, float Pos1, float Time2, float Pos2)
{
	// Linear interpolation to find zero crossing
	// t = t1 + (0 - p1) * (t2 - t1) / (p2 - p1)
	float DeltaPos = Pos2 - Pos1;

	if (FMath::Abs(DeltaPos) < KINDA_SMALL_NUMBER)
	{
		return (Time1 + Time2) * 0.5f;  // Return midpoint if positions are very close
	}

	float t = Time1 + (-Pos1) * (Time2 - Time1) / DeltaPos;
	return FMath::Clamp(t, Time1, Time2);
}

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

	// Collect 3D relative positions for all frames
	TArray<FVector> RelativePositions;
	for (int32 i = 0; i < Poses.Num(); ++i)
	{
		RelativePositions.Add(GetFootRelativePosition(Poses[i], Preset.PelvisBoneName, Foot.BoneName));
	}

	// Determine primary movement axis from foot trajectory
	FVector MoveAxis = DeterminePrimaryMoveAxis(RelativePositions);

	// Project positions onto the determined move axis
	TArray<float> Positions;
	for (const FVector& RelPos : RelativePositions)
	{
		Positions.Add(FVector::DotProduct(RelPos, MoveAxis));
	}

	// Find zero crossings (pelvis line crossings along the determined axis)
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
			float LastTime = static_cast<float>(TimeIntervals.Last());

			bool bIsContact = (LastPos < 0.0f && FirstPos > 0.0f);

			Results.Add(FFootContactResult(
				LastTime,
				Settings->LoopBoundaryConfidence,
				bIsContact,
				EFootContactDetectionMethod::PelvisCrossing
			));
		}
	}

	return Results;
}

FVector FPelvisCrossingDetector::GetFootRelativePosition(
	const FAnimPose& Pose,
	FName PelvisBone,
	FName FootBone)
{
	// Get relative transform from pelvis to foot in world space
	FTransform RelativeTransform = UAnimPoseExtensions::GetRelativeTransform(
		Pose, PelvisBone, FootBone, EAnimPoseSpaces::World);

	return RelativeTransform.GetLocation();
}

FVector FPelvisCrossingDetector::DeterminePrimaryMoveAxis(const TArray<FVector>& Positions)
{
	if (Positions.Num() < 2)
	{
		return FVector::ForwardVector;
	}

	// Find min/max for X and Y axes
	float MinX = TNumericLimits<float>::Max();
	float MaxX = TNumericLimits<float>::Lowest();
	float MinY = TNumericLimits<float>::Max();
	float MaxY = TNumericLimits<float>::Lowest();

	for (const FVector& Pos : Positions)
	{
		MinX = FMath::Min(MinX, Pos.X);
		MaxX = FMath::Max(MaxX, Pos.X);
		MinY = FMath::Min(MinY, Pos.Y);
		MaxY = FMath::Max(MaxY, Pos.Y);
	}

	float XRange = MaxX - MinX;
	float YRange = MaxY - MinY;

	// Choose the axis with greater range as the primary movement axis
	if (YRange > XRange)
	{
		// Y-axis dominant (strafing)
		return FVector::RightVector;
	}
	else
	{
		// X-axis dominant (forward/backward)
		return FVector::ForwardVector;
	}
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

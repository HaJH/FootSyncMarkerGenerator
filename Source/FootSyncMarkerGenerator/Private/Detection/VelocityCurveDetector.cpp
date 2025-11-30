// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#include "Detection/VelocityCurveDetector.h"
#include "FootSyncMarkerSettings.h"
#include "AnimPose.h"
#include "AnimationBlueprintLibrary.h"
#include "Animation/AnimSequence.h"

TArray<FFootContactResult> FVelocityCurveDetector::DetectContacts(
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

	if (NumKeys < 3)  // Need at least 3 frames to find minima
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
			TEXT("VelocityCurveDetector: Pose count mismatch"));
		return Results;
	}

	// Calculate velocities
	TArray<float> Velocities = CalculateVelocities(Poses, Times, Foot.BoneName);

	if (Velocities.Num() < 3)
	{
		return Results;
	}

	// Find local minima
	const float Threshold = bHasThresholdOverride ? VelocityThresholdOverride : Settings->VelocityMinimumThreshold;
	TArray<int32> MinimaIndices = FindLocalMinima(Velocities, Threshold);

	// Calculate max velocity for confidence scaling
	float MaxVelocity = 0.0f;
	for (float Vel : Velocities)
	{
		MaxVelocity = FMath::Max(MaxVelocity, Vel);
	}

	// Convert minima to results
	for (int32 Index : MinimaIndices)
	{
		if (Index >= 0 && Index < Times.Num())
		{
			float Velocity = Velocities[Index];

			// Higher confidence for lower velocities
			float Confidence = MaxVelocity > KINDA_SMALL_NUMBER
				? 1.0f - FMath::Clamp(Velocity / MaxVelocity, 0.0f, 0.9f)
				: Settings->VelocityDefaultConfidence;

			Results.Add(FFootContactResult(
				Times[Index],
				Confidence,
				true,  // Velocity minima indicate foot contact
				EFootContactDetectionMethod::VelocityCurve
			));
		}
	}

	return Results;
}

TArray<float> FVelocityCurveDetector::CalculateVelocities(
	const TArray<FAnimPose>& Poses,
	const TArray<float>& Times,
	FName FootBone)
{
	TArray<float> Velocities;

	if (Poses.Num() < 2)
	{
		return Velocities;
	}

	// Get positions in world space
	TArray<FVector> Positions;
	for (const FAnimPose& Pose : Poses)
	{
		FTransform FootTransform = UAnimPoseExtensions::GetBonePose(
			Pose, FootBone, EAnimPoseSpaces::World);
		Positions.Add(FootTransform.GetLocation());
	}

	// Calculate velocities (central difference for interior points, forward/backward for edges)
	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		float Velocity;

		if (i == 0)
		{
			// Forward difference
			float DeltaTime = Times[1] - Times[0];
			if (DeltaTime > KINDA_SMALL_NUMBER)
			{
				Velocity = (Positions[1] - Positions[0]).Size() / DeltaTime;
			}
			else
			{
				Velocity = 0.0f;
			}
		}
		else if (i == Positions.Num() - 1)
		{
			// Backward difference
			float DeltaTime = Times[i] - Times[i - 1];
			if (DeltaTime > KINDA_SMALL_NUMBER)
			{
				Velocity = (Positions[i] - Positions[i - 1]).Size() / DeltaTime;
			}
			else
			{
				Velocity = 0.0f;
			}
		}
		else
		{
			// Central difference (average of forward and backward)
			float DeltaTime1 = Times[i] - Times[i - 1];
			float DeltaTime2 = Times[i + 1] - Times[i];
			float TotalDeltaTime = DeltaTime1 + DeltaTime2;

			if (TotalDeltaTime > KINDA_SMALL_NUMBER)
			{
				Velocity = (Positions[i + 1] - Positions[i - 1]).Size() / TotalDeltaTime;
			}
			else
			{
				Velocity = 0.0f;
			}
		}

		Velocities.Add(Velocity);
	}

	return Velocities;
}

TArray<int32> FVelocityCurveDetector::FindLocalMinima(
	const TArray<float>& Velocities,
	float Threshold)
{
	TArray<int32> MinimaIndices;

	if (Velocities.Num() < 3)
	{
		return MinimaIndices;
	}

	for (int32 i = 1; i < Velocities.Num() - 1; ++i)
	{
		float Prev = Velocities[i - 1];
		float Curr = Velocities[i];
		float Next = Velocities[i + 1];

		// Check if current is a local minimum
		if (Curr < Prev && Curr < Next)
		{
			// Only add if below threshold (to filter out noise during foot movement)
			if (Curr < Threshold)
			{
				MinimaIndices.Add(i);
			}
		}
		// Also check for flat regions (plateau minima)
		else if (Curr <= Prev && Curr < Next && Curr < Threshold)
		{
			MinimaIndices.Add(i);
		}
		else if (Curr < Prev && Curr <= Next && Curr < Threshold)
		{
			MinimaIndices.Add(i);
		}
	}

	// Check edges if they're very low velocity
	if (Velocities[0] < Threshold && Velocities[0] < Velocities[1])
	{
		MinimaIndices.Insert(0, 0);
	}

	int32 LastIndex = Velocities.Num() - 1;
	if (Velocities[LastIndex] < Threshold && Velocities[LastIndex] < Velocities[LastIndex - 1])
	{
		MinimaIndices.Add(LastIndex);
	}

	return MinimaIndices;
}

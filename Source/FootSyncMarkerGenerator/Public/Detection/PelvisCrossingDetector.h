// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IFootContactDetector.h"

/**
 * Detects foot contacts by finding when the foot crosses the pelvis line
 * Based on gportelli/FootSyncMarkers algorithm
 */
class FOOTSYNCMARKERGENERATOR_API FPelvisCrossingDetector : public IFootContactDetector
{
public:
	FPelvisCrossingDetector() = default;
	virtual ~FPelvisCrossingDetector() = default;

	// IFootContactDetector interface
	virtual TArray<FFootContactResult> DetectContacts(
		UAnimSequence* AnimSequence,
		const FSyncFootDefinition& Foot,
		const FLocomotionPreset& Preset) override;

	virtual FString GetDetectorName() const override { return TEXT("PelvisCrossing"); }

private:
	/**
	 * Get the foot position relative to pelvis along the move axis
	 * @param Pose Animation pose at a given time
	 * @param PelvisBone Name of the pelvis bone
	 * @param FootBone Name of the foot bone
	 * @param MoveAxis Primary movement axis
	 * @return Distance along the move axis (positive = forward, negative = backward)
	 */
	float GetFootPositionAlongAxis(
		const struct FAnimPose& Pose,
		FName PelvisBone,
		FName FootBone,
		const FVector& MoveAxis);

	/**
	 * Interpolate the exact crossing time between two frames
	 * @param Time1 Time of first frame
	 * @param Pos1 Position at first frame
	 * @param Time2 Time of second frame
	 * @param Pos2 Position at second frame
	 * @return Interpolated time when position crosses zero
	 */
	float InterpolateCrossingTime(float Time1, float Pos1, float Time2, float Pos2);
};

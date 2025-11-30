// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IFootContactDetector.h"

/**
 * Detects foot contacts by finding velocity minima
 * When the foot is planted, its velocity approaches zero
 */
class FOOTSYNCMARKERGENERATOR_API FVelocityCurveDetector : public IFootContactDetector
{
public:
	FVelocityCurveDetector() = default;
	virtual ~FVelocityCurveDetector() = default;

	// IFootContactDetector interface
	virtual TArray<FFootContactResult> DetectContacts(
		UAnimSequence* AnimSequence,
		const FSyncFootDefinition& Foot,
		const FLocomotionPreset& Preset) override;

	virtual FString GetDetectorName() const override { return TEXT("VelocityCurve"); }

	virtual void SetVelocityThreshold(float Threshold) override { VelocityThresholdOverride = Threshold; bHasThresholdOverride = true; }

private:
	bool bHasThresholdOverride = false;
	float VelocityThresholdOverride = 0.0f;

	/**
	 * Calculate foot velocities from pose data
	 * @param Poses Array of poses
	 * @param Times Time at each pose
	 * @param FootBone Name of the foot bone
	 * @return Array of velocities (cm/s)
	 */
	TArray<float> CalculateVelocities(
		const TArray<struct FAnimPose>& Poses,
		const TArray<float>& Times,
		FName FootBone);

	/**
	 * Find local minima in the velocity data
	 * @param Velocities Array of velocities
	 * @param Times Time at each velocity sample
	 * @param Threshold Minimum velocity to consider (filters noise)
	 * @return Indices of local minima
	 */
	TArray<int32> FindLocalMinima(
		const TArray<float>& Velocities,
		float Threshold);
};

// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LocomotionPresets.h"

class UAnimSequence;

/**
 * Interface for foot contact detection algorithms
 */
class FOOTSYNCMARKERGENERATOR_API IFootContactDetector
{
public:
	virtual ~IFootContactDetector() = default;

	/**
	 * Detect foot contact times in the given animation sequence
	 * @param AnimSequence Animation sequence to analyze
	 * @param Foot Foot definition (bone name, etc.)
	 * @param Preset Locomotion preset (pelvis bone, move axis, etc.)
	 * @return Array of contact results with times and confidence
	 */
	virtual TArray<FFootContactResult> DetectContacts(
		UAnimSequence* AnimSequence,
		const FSyncFootDefinition& Foot,
		const FLocomotionPreset& Preset) = 0;

	/** Get detector name for logging/debugging */
	virtual FString GetDetectorName() const = 0;

	/** Set velocity threshold (optional, for detectors that use it) */
	virtual void SetVelocityThreshold(float Threshold) {}

	/** Set saliency threshold (optional, for detectors that use it) */
	virtual void SetSaliencyThreshold(float Threshold) {}
};

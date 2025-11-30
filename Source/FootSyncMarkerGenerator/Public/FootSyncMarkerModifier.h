// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "LocomotionPresets.h"
#include "FootSyncMarkerModifier.generated.h"

class IFootContactDetector;

/**
 * Animation Modifier that automatically generates foot sync markers
 */
UCLASS(BlueprintType, meta = (DisplayName = "FootSync Marker Generator"))
class FOOTSYNCMARKERGENERATOR_API UFootSyncMarkerModifier : public UAnimationModifier
{
	GENERATED_BODY()

public:
	UFootSyncMarkerModifier();

	// UAnimationModifier interface
	virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) override;
	virtual void OnRevert_Implementation(UAnimSequence* AnimationSequence) override;

	// ============== Locomotion Settings ==============

	/** Type of locomotion (determines default foot configuration) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	ELocomotionType LocomotionType = ELocomotionType::Bipedal;

	/** Custom preset (used when LocomotionType is Custom) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion",
		meta = (EditCondition = "LocomotionType == ELocomotionType::Custom"))
	FLocomotionPreset CustomPreset;

	// ============== Detection Settings ==============

	/** Whether to override the global detection method */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	bool bOverrideDetectionMethod = false;

	/** Detection method override (when bOverrideDetectionMethod is true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection",
		meta = (EditCondition = "bOverrideDetectionMethod"))
	EFootContactDetectionMethod DetectionMethodOverride = EFootContactDetectionMethod::Composite;

	/** Whether to override minimum confidence threshold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	bool bOverrideMinimumConfidence = false;

	/** Minimum confidence threshold for marker creation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection",
		meta = (EditCondition = "bOverrideMinimumConfidence", ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumConfidenceOverride = 0.3f;

	/** Whether to override velocity minimum threshold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	bool bOverrideVelocityThreshold = false;

	/** Minimum velocity threshold for foot contact detection (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection",
		meta = (EditCondition = "bOverrideVelocityThreshold", ClampMin = "0.1", ClampMax = "100.0"))
	float VelocityThresholdOverride = 5.0f;

	/** Whether to override saliency threshold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection")
	bool bOverrideSaliencyThreshold = false;

	/** Threshold for saliency point detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection",
		meta = (EditCondition = "bOverrideSaliencyThreshold", ClampMin = "0.0", ClampMax = "1.0"))
	float SaliencyThresholdOverride = 0.5f;

	// ============== Marker Settings ==============

	/** Whether to override max markers per foot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	bool bOverrideMaxMarkersPerFoot = false;

	/** Maximum markers per foot (0 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker",
		meta = (EditCondition = "bOverrideMaxMarkersPerFoot", ClampMin = "0"))
	int32 MaxMarkersPerFootOverride = 2;

	/** Whether to override guarantee minimum one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	bool bOverrideGuaranteeMinimumOne = false;

	/** Guarantee at least one marker per foot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker",
		meta = (EditCondition = "bOverrideGuaranteeMinimumOne"))
	bool bGuaranteeMinimumOneOverride = true;

	// ============== Output Settings ==============

	/** Whether to generate distance curves (pelvis-to-foot distance) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	bool bGenerateDistanceCurves = true;

	/** Whether to generate velocity curves */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	bool bGenerateVelocityCurves = false;

protected:
	/**
	 * Process the animation sequence with the given preset
	 */
	void ProcessAnimation(UAnimSequence* AnimSequence, const FLocomotionPreset& Preset);

	/**
	 * Detect foot contacts using the configured detection method
	 */
	TArray<FFootContactResult> DetectFootContacts(
		UAnimSequence* AnimSequence,
		const FSyncFootDefinition& Foot,
		const FLocomotionPreset& Preset);

	/**
	 * Add sync markers to the animation sequence
	 */
	void AddSyncMarkers(
		UAnimSequence* AnimSequence,
		const FSyncFootDefinition& Foot,
		const TArray<float>& ContactTimes);

	/**
	 * Generate distance and/or velocity curves
	 */
	void GenerateCurves(
		UAnimSequence* AnimSequence,
		const FSyncFootDefinition& Foot,
		const FLocomotionPreset& Preset);

	/**
	 * Remove all generated data (markers and curves) from the animation
	 */
	void RemoveGeneratedData(UAnimSequence* AnimSequence, const FLocomotionPreset& Preset);

	/**
	 * Create a detector instance for the given method
	 */
	TUniquePtr<IFootContactDetector> CreateDetector(EFootContactDetectionMethod Method);

	/**
	 * Get the preset to use (either from settings or custom)
	 */
	FLocomotionPreset GetEffectivePreset(UAnimSequence* AnimSequence) const;

	/**
	 * Get the detection method to use
	 */
	EFootContactDetectionMethod GetEffectiveDetectionMethod() const;

	/** Get effective minimum confidence (override or global) */
	float GetEffectiveMinimumConfidence() const;

	/** Get effective velocity threshold (override or global) */
	float GetEffectiveVelocityThreshold() const;

	/** Get effective saliency threshold (override or global) */
	float GetEffectiveSaliencyThreshold() const;

	/** Get effective max markers per foot (override or global) */
	int32 GetEffectiveMaxMarkersPerFoot() const;

	/** Get effective guarantee minimum one (override or global) */
	bool GetEffectiveGuaranteeMinimumOne() const;
};

// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LocomotionPresets.h"
#include "FootSyncMarkerSettings.generated.h"

class USkeleton;

/**
 * Project settings for FootSync Marker Generator
 */
UCLASS(config = EditorPerProjectUserSettings, defaultconfig,
	meta = (DisplayName = "FootSync Marker Generator"))
class FOOTSYNCMARKERGENERATOR_API UFootSyncMarkerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFootSyncMarkerSettings();

	static UFootSyncMarkerSettings* Get()
	{
		UFootSyncMarkerSettings* Settings = GetMutableDefault<UFootSyncMarkerSettings>();
		check(Settings);
		return Settings;
	}

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	// ============== Detection Settings ==============

	/** Default detection method */
	UPROPERTY(config, EditAnywhere, Category = "Detection")
	EFootContactDetectionMethod DetectionMethod = EFootContactDetectionMethod::Composite;

	/** Minimum confidence threshold for marker creation */
	UPROPERTY(config, EditAnywhere, Category = "Detection",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumConfidence = 0.3f;

	/** Weights for composite detection */
	UPROPERTY(config, EditAnywhere, Category = "Detection",
		meta = (EditCondition = "DetectionMethod == EFootContactDetectionMethod::Composite"))
	FCompositeDetectionWeights CompositeWeights;

	// ============== Pelvis Crossing Detection ==============

	/** Threshold for pelvis line crossing detection (cm) */
	UPROPERTY(config, EditAnywhere, Category = "Detection|Pelvis Crossing",
		meta = (ClampMin = "0.001", ClampMax = "10.0"))
	float CrossingThreshold = 0.01f;

	/** Position change divisor for confidence calculation (cm) */
	UPROPERTY(config, EditAnywhere, Category = "Detection|Pelvis Crossing",
		meta = (ClampMin = "1.0", ClampMax = "200.0"))
	float PelvisConfidenceScale = 50.0f;

	/** Confidence for loop boundary crossings */
	UPROPERTY(config, EditAnywhere, Category = "Detection|Pelvis Crossing",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LoopBoundaryConfidence = 0.7f;

	// ============== Velocity Curve Detection ==============

	/** Minimum velocity threshold for foot contact detection (cm/s) */
	UPROPERTY(config, EditAnywhere, Category = "Detection|Velocity Curve",
		meta = (ClampMin = "0.1", ClampMax = "100.0"))
	float VelocityMinimumThreshold = 5.0f;

	/** Default confidence when max velocity is zero */
	UPROPERTY(config, EditAnywhere, Category = "Detection|Velocity Curve",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VelocityDefaultConfidence = 0.5f;

	// ============== Saliency Detection ==============

	/** Analysis window size for saliency detection (seconds) */
	UPROPERTY(config, EditAnywhere, Category = "Detection|Saliency",
		meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SaliencyWindowSize = 0.1f;

	/** Threshold for saliency point detection (0.0 - 1.0) */
	UPROPERTY(config, EditAnywhere, Category = "Detection|Saliency",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SaliencyThreshold = 0.5f;

	/** Default confidence when max curvature is zero */
	UPROPERTY(config, EditAnywhere, Category = "Detection|Saliency",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SaliencyDefaultConfidence = 0.5f;

	/** Minimum confidence for saliency detection */
	UPROPERTY(config, EditAnywhere, Category = "Detection|Saliency",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SaliencyMinConfidence = 0.3f;

	// ============== Output Settings ==============

	/** Name of the sync marker track */
	UPROPERTY(config, EditAnywhere, Category = "Output")
	FName SyncMarkerTrackName = TEXT("FootSync");

	/** Maximum markers per foot (0 = unlimited) */
	UPROPERTY(config, EditAnywhere, Category = "Output",
		meta = (ClampMin = "0"))
	int32 MaxMarkersPerFoot = 2;

	/** Guarantee at least one marker per foot even if below confidence threshold */
	UPROPERTY(config, EditAnywhere, Category = "Output")
	bool bGuaranteeMinimumOne = true;

	/** Primary move axis Z component for flying locomotion */
	UPROPERTY(config, EditAnywhere, Category = "Output|Flying",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlyingMoveAxisZ = 0.3f;

	/** Marker naming convention settings */
	UPROPERTY(config, EditAnywhere, Category = "Output")
	FFootMarkerNameSettings MarkerNameSettings;

	/** Whether to generate distance curves (pelvis-to-foot distance) */
	UPROPERTY(config, EditAnywhere, Category = "Output")
	bool bGenerateDistanceCurves = true;

	/** Whether to generate velocity curves */
	UPROPERTY(config, EditAnywhere, Category = "Output")
	bool bGenerateVelocityCurves = false;

	/** Suffix for distance curves */
	UPROPERTY(config, EditAnywhere, Category = "Output",
		meta = (EditCondition = "bGenerateDistanceCurves"))
	FString DistanceCurveSuffix = TEXT("_Distance");

	/** Suffix for velocity curves */
	UPROPERTY(config, EditAnywhere, Category = "Output",
		meta = (EditCondition = "bGenerateVelocityCurves"))
	FString VelocityCurveSuffix = TEXT("_Velocity");

	// ============== Bone Matching Patterns ==============

	/** Patterns to match pelvis/hip bones (case-insensitive contains match) */
	UPROPERTY(config, EditAnywhere, Category = "Bone Matching")
	TArray<FString> PelvisBonePatterns;

	/** Patterns to match left foot bones */
	UPROPERTY(config, EditAnywhere, Category = "Bone Matching")
	TArray<FString> LeftFootBonePatterns;

	/** Patterns to match right foot bones */
	UPROPERTY(config, EditAnywhere, Category = "Bone Matching")
	TArray<FString> RightFootBonePatterns;

	/** Patterns to match front-left foot bones (quadruped) */
	UPROPERTY(config, EditAnywhere, Category = "Bone Matching|Quadruped")
	TArray<FString> FrontLeftFootPatterns;

	/** Patterns to match front-right foot bones (quadruped) */
	UPROPERTY(config, EditAnywhere, Category = "Bone Matching|Quadruped")
	TArray<FString> FrontRightFootPatterns;

	// ============== Advanced Settings ==============

	/** Time threshold for merging nearby detection results (seconds) */
	UPROPERTY(config, EditAnywhere, Category = "Advanced",
		meta = (ClampMin = "0.001", ClampMax = "0.5"))
	float ResultMergeThreshold = 0.05f;

	/** Minimum time between consecutive markers for the same foot (seconds) */
	UPROPERTY(config, EditAnywhere, Category = "Advanced",
		meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float MinimumMarkerInterval = 0.1f;

	/** Confidence bonus per additional detector agreement (Composite) */
	UPROPERTY(config, EditAnywhere, Category = "Advanced",
		meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float DetectorAgreementBonus = 0.1f;

	// ============== Helper Functions ==============

	/** Find pelvis bone from skeleton using patterns */
	FName FindPelvisBone(const USkeleton* Skeleton) const;

	/** Find a foot bone from skeleton using the given patterns */
	FName FindFootBone(const USkeleton* Skeleton, const TArray<FString>& Patterns) const;

	/** Create a preset for the given skeleton and locomotion type */
	FLocomotionPreset CreatePresetForSkeleton(const USkeleton* Skeleton, ELocomotionType Type) const;

	/** Reset bone patterns to defaults */
	UFUNCTION(CallInEditor, Category = "Bone Matching")
	void ResetToDefaultPatterns();

private:
	void InitializeDefaultPatterns();
};

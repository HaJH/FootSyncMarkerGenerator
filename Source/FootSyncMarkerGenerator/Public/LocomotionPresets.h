// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LocomotionPresets.generated.h"

/**
 * Locomotion type for foot sync marker generation
 */
UENUM(BlueprintType)
enum class ELocomotionType : uint8
{
	Bipedal			UMETA(DisplayName = "Bipedal (2 feet)"),
	HumanoidFlying	UMETA(DisplayName = "Humanoid Flying"),
	Quadruped		UMETA(DisplayName = "Quadruped (4 feet)"),
	Custom			UMETA(DisplayName = "Custom")
};

/**
 * Detection method for foot contact detection
 */
UENUM(BlueprintType)
enum class EFootContactDetectionMethod : uint8
{
	PelvisCrossing	UMETA(DisplayName = "Pelvis Line Crossing"),
	VelocityCurve	UMETA(DisplayName = "Velocity Curve"),
	Saliency		UMETA(DisplayName = "Saliency Based"),
	Composite		UMETA(DisplayName = "Composite (All Combined)")
};

/**
 * Foot label for identification
 */
UENUM(BlueprintType)
enum class EFootLabel : uint8
{
	Left,
	Right,
	FrontLeft,
	FrontRight,
	BackLeft,
	BackRight,
	Custom
};

/**
 * Definition of a single foot for sync marker generation
 */
USTRUCT(BlueprintType)
struct FOOTSYNCMARKERGENERATOR_API FSyncFootDefinition
{
	GENERATED_BODY()

	/** Bone name for this foot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot")
	FName BoneName;

	/** Sync marker name (e.g., "FootDown_L") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot")
	FName MarkerName;

	/** Foot label for identification */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot")
	EFootLabel FootLabel = EFootLabel::Left;

	/** Custom label when FootLabel is Custom */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foot",
		meta = (EditCondition = "FootLabel == EFootLabel::Custom"))
	FString CustomLabel;

	FSyncFootDefinition()
		: BoneName(NAME_None)
		, MarkerName(NAME_None)
		, FootLabel(EFootLabel::Left)
	{
	}

	FSyncFootDefinition(FName InBoneName, FName InMarkerName, EFootLabel InLabel)
		: BoneName(InBoneName)
		, MarkerName(InMarkerName)
		, FootLabel(InLabel)
	{
	}
};

/**
 * Settings for marker naming convention
 */
USTRUCT(BlueprintType)
struct FOOTSYNCMARKERGENERATOR_API FFootMarkerNameSettings
{
	GENERATED_BODY()

	/** Prefix for all foot markers (e.g., "FootDown") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naming")
	FString MarkerPrefix = TEXT("FootDown");

	/** Suffix for left foot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naming")
	FString LeftSuffix = TEXT("_L");

	/** Suffix for right foot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naming")
	FString RightSuffix = TEXT("_R");

	/** Suffix for front-left foot (quadruped) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naming")
	FString FrontLeftSuffix = TEXT("_FL");

	/** Suffix for front-right foot (quadruped) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naming")
	FString FrontRightSuffix = TEXT("_FR");

	/** Suffix for back-left foot (quadruped) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naming")
	FString BackLeftSuffix = TEXT("_BL");

	/** Suffix for back-right foot (quadruped) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Naming")
	FString BackRightSuffix = TEXT("_BR");

	/** Generate marker name for the given foot label */
	FName GetMarkerName(EFootLabel Label) const
	{
		FString Suffix;
		switch (Label)
		{
		case EFootLabel::Left:			Suffix = LeftSuffix; break;
		case EFootLabel::Right:			Suffix = RightSuffix; break;
		case EFootLabel::FrontLeft:		Suffix = FrontLeftSuffix; break;
		case EFootLabel::FrontRight:	Suffix = FrontRightSuffix; break;
		case EFootLabel::BackLeft:		Suffix = BackLeftSuffix; break;
		case EFootLabel::BackRight:		Suffix = BackRightSuffix; break;
		default: break;
		}
		return FName(*(MarkerPrefix + Suffix));
	}
};

/**
 * Weights for composite detection method
 */
USTRUCT(BlueprintType)
struct FOOTSYNCMARKERGENERATOR_API FCompositeDetectionWeights
{
	GENERATED_BODY()

	/** Weight for pelvis crossing detection (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PelvisCrossingWeight = 0.4f;

	/** Weight for velocity curve detection (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VelocityCurveWeight = 0.3f;

	/** Weight for saliency detection (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weights",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SaliencyWeight = 0.3f;
};

/**
 * Locomotion preset for a specific skeleton type
 */
USTRUCT(BlueprintType)
struct FOOTSYNCMARKERGENERATOR_API FLocomotionPreset
{
	GENERATED_BODY()

	/** Type of locomotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	ELocomotionType Type = ELocomotionType::Bipedal;

	/** Pelvis/Hip bone name (reference point) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FName PelvisBoneName;

	/** List of foot definitions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	TArray<FSyncFootDefinition> Feet;

	/** Primary movement axis (Forward direction in character space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FVector PrimaryMoveAxis = FVector::ForwardVector;

	/** Whether this preset is valid */
	bool IsValid() const
	{
		return !PelvisBoneName.IsNone() && Feet.Num() > 0;
	}
};

/**
 * Result of foot contact detection
 */
USTRUCT(BlueprintType)
struct FOOTSYNCMARKERGENERATOR_API FFootContactResult
{
	GENERATED_BODY()

	/** Time of contact in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	float Time = 0.0f;

	/** Confidence level (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	float Confidence = 0.0f;

	/** Whether this is a contact (true) or lift-off (false) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	bool bIsContact = true;

	/** Source detection method */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	EFootContactDetectionMethod Source = EFootContactDetectionMethod::PelvisCrossing;

	FFootContactResult() = default;

	FFootContactResult(float InTime, float InConfidence, bool bInIsContact,
		EFootContactDetectionMethod InSource = EFootContactDetectionMethod::PelvisCrossing)
		: Time(InTime)
		, Confidence(InConfidence)
		, bIsContact(bInIsContact)
		, Source(InSource)
	{
	}
};

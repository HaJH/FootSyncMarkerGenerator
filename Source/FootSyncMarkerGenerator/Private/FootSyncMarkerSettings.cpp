// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#include "FootSyncMarkerSettings.h"
#include "Animation/Skeleton.h"

UFootSyncMarkerSettings::UFootSyncMarkerSettings()
{
	if (PelvisBonePatterns.Num() == 0)
	{
		InitializeDefaultPatterns();
	}
}

void UFootSyncMarkerSettings::InitializeDefaultPatterns()
{
	// Pelvis patterns
	PelvisBonePatterns.Empty();
	PelvisBonePatterns.Add(TEXT("pelvis"));
	PelvisBonePatterns.Add(TEXT("Pelvis"));
	PelvisBonePatterns.Add(TEXT("hips"));
	PelvisBonePatterns.Add(TEXT("Hips"));
	PelvisBonePatterns.Add(TEXT("hip"));
	PelvisBonePatterns.Add(TEXT("Hip"));

	// Left foot patterns
	LeftFootBonePatterns.Empty();
	LeftFootBonePatterns.Add(TEXT("foot_l"));
	LeftFootBonePatterns.Add(TEXT("Foot_L"));
	LeftFootBonePatterns.Add(TEXT("LeftFoot"));
	LeftFootBonePatterns.Add(TEXT("Left_Foot"));
	LeftFootBonePatterns.Add(TEXT("l_foot"));
	LeftFootBonePatterns.Add(TEXT("L_Foot"));
	LeftFootBonePatterns.Add(TEXT("foot_left"));

	// Right foot patterns
	RightFootBonePatterns.Empty();
	RightFootBonePatterns.Add(TEXT("foot_r"));
	RightFootBonePatterns.Add(TEXT("Foot_R"));
	RightFootBonePatterns.Add(TEXT("RightFoot"));
	RightFootBonePatterns.Add(TEXT("Right_Foot"));
	RightFootBonePatterns.Add(TEXT("r_foot"));
	RightFootBonePatterns.Add(TEXT("R_Foot"));
	RightFootBonePatterns.Add(TEXT("foot_right"));

	// Quadruped front-left patterns (often "hand" in animal rigs)
	FrontLeftFootPatterns.Empty();
	FrontLeftFootPatterns.Add(TEXT("front_foot_l"));
	FrontLeftFootPatterns.Add(TEXT("FrontFoot_L"));
	FrontLeftFootPatterns.Add(TEXT("hand_l"));
	FrontLeftFootPatterns.Add(TEXT("Hand_L"));
	FrontLeftFootPatterns.Add(TEXT("paw_fl"));
	FrontLeftFootPatterns.Add(TEXT("front_paw_l"));
	FrontLeftFootPatterns.Add(TEXT("LeftHand"));

	// Quadruped front-right patterns
	FrontRightFootPatterns.Empty();
	FrontRightFootPatterns.Add(TEXT("front_foot_r"));
	FrontRightFootPatterns.Add(TEXT("FrontFoot_R"));
	FrontRightFootPatterns.Add(TEXT("hand_r"));
	FrontRightFootPatterns.Add(TEXT("Hand_R"));
	FrontRightFootPatterns.Add(TEXT("paw_fr"));
	FrontRightFootPatterns.Add(TEXT("front_paw_r"));
	FrontRightFootPatterns.Add(TEXT("RightHand"));
}

void UFootSyncMarkerSettings::ResetToDefaultPatterns()
{
	InitializeDefaultPatterns();
	SaveConfig();
}

FName UFootSyncMarkerSettings::FindPelvisBone(const USkeleton* Skeleton) const
{
	if (!Skeleton)
	{
		return NAME_None;
	}

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
	{
		const FString BoneName = RefSkeleton.GetBoneName(BoneIndex).ToString();

		for (const FString& Pattern : PelvisBonePatterns)
		{
			if (BoneName.Contains(Pattern, ESearchCase::IgnoreCase))
			{
				return RefSkeleton.GetBoneName(BoneIndex);
			}
		}
	}

	return NAME_None;
}

FName UFootSyncMarkerSettings::FindFootBone(const USkeleton* Skeleton, const TArray<FString>& Patterns) const
{
	if (!Skeleton || Patterns.Num() == 0)
	{
		return NAME_None;
	}

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
	{
		const FString BoneName = RefSkeleton.GetBoneName(BoneIndex).ToString();

		for (const FString& Pattern : Patterns)
		{
			if (BoneName.Contains(Pattern, ESearchCase::IgnoreCase))
			{
				return RefSkeleton.GetBoneName(BoneIndex);
			}
		}
	}

	return NAME_None;
}

FLocomotionPreset UFootSyncMarkerSettings::CreatePresetForSkeleton(
	const USkeleton* Skeleton, ELocomotionType Type) const
{
	FLocomotionPreset Preset;
	Preset.Type = Type;
	Preset.PelvisBoneName = FindPelvisBone(Skeleton);
	Preset.PrimaryMoveAxis = FVector::ForwardVector;

	switch (Type)
	{
	case ELocomotionType::Bipedal:
		{
			FName LeftFoot = FindFootBone(Skeleton, LeftFootBonePatterns);
			FName RightFoot = FindFootBone(Skeleton, RightFootBonePatterns);

			if (!LeftFoot.IsNone())
			{
				Preset.Feet.Add(FSyncFootDefinition(
					LeftFoot,
					MarkerNameSettings.GetMarkerName(EFootLabel::Left),
					EFootLabel::Left
				));
			}

			if (!RightFoot.IsNone())
			{
				Preset.Feet.Add(FSyncFootDefinition(
					RightFoot,
					MarkerNameSettings.GetMarkerName(EFootLabel::Right),
					EFootLabel::Right
				));
			}
		}
		break;

	case ELocomotionType::HumanoidFlying:
		{
			// Same as bipedal but may use different detection axis
			FName LeftFoot = FindFootBone(Skeleton, LeftFootBonePatterns);
			FName RightFoot = FindFootBone(Skeleton, RightFootBonePatterns);

			if (!LeftFoot.IsNone())
			{
				Preset.Feet.Add(FSyncFootDefinition(
					LeftFoot,
					MarkerNameSettings.GetMarkerName(EFootLabel::Left),
					EFootLabel::Left
				));
			}

			if (!RightFoot.IsNone())
			{
				Preset.Feet.Add(FSyncFootDefinition(
					RightFoot,
					MarkerNameSettings.GetMarkerName(EFootLabel::Right),
					EFootLabel::Right
				));
			}

			// For flying, consider both forward and up axes
			Preset.PrimaryMoveAxis = FVector(1.0f, 0.0f, FlyingMoveAxisZ).GetSafeNormal();
		}
		break;

	case ELocomotionType::Quadruped:
		{
			// Front feet
			FName FrontLeft = FindFootBone(Skeleton, FrontLeftFootPatterns);
			FName FrontRight = FindFootBone(Skeleton, FrontRightFootPatterns);

			// Back feet (use regular left/right patterns)
			FName BackLeft = FindFootBone(Skeleton, LeftFootBonePatterns);
			FName BackRight = FindFootBone(Skeleton, RightFootBonePatterns);

			if (!FrontLeft.IsNone())
			{
				Preset.Feet.Add(FSyncFootDefinition(
					FrontLeft,
					MarkerNameSettings.GetMarkerName(EFootLabel::FrontLeft),
					EFootLabel::FrontLeft
				));
			}

			if (!FrontRight.IsNone())
			{
				Preset.Feet.Add(FSyncFootDefinition(
					FrontRight,
					MarkerNameSettings.GetMarkerName(EFootLabel::FrontRight),
					EFootLabel::FrontRight
				));
			}

			if (!BackLeft.IsNone())
			{
				Preset.Feet.Add(FSyncFootDefinition(
					BackLeft,
					MarkerNameSettings.GetMarkerName(EFootLabel::BackLeft),
					EFootLabel::BackLeft
				));
			}

			if (!BackRight.IsNone())
			{
				Preset.Feet.Add(FSyncFootDefinition(
					BackRight,
					MarkerNameSettings.GetMarkerName(EFootLabel::BackRight),
					EFootLabel::BackRight
				));
			}
		}
		break;

	case ELocomotionType::Custom:
		// Custom presets are defined by the user
		break;
	}

	return Preset;
}

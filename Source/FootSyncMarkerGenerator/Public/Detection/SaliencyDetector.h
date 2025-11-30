// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IFootContactDetector.h"

/**
 * Detects foot contacts by analyzing trajectory curvature (saliency)
 * Based on IEEE paper: "Foot plant detection by curve saliency"
 *
 * The algorithm finds salient points where the trajectory curvature changes rapidly,
 * indicating the transition between foot plant and movement phases.
 */
class FOOTSYNCMARKERGENERATOR_API FSaliencyDetector : public IFootContactDetector
{
public:
	FSaliencyDetector() = default;
	virtual ~FSaliencyDetector() = default;

	// IFootContactDetector interface
	virtual TArray<FFootContactResult> DetectContacts(
		UAnimSequence* AnimSequence,
		const FSyncFootDefinition& Foot,
		const FLocomotionPreset& Preset) override;

	virtual FString GetDetectorName() const override { return TEXT("Saliency"); }

	virtual void SetSaliencyThreshold(float Threshold) override { SaliencyThresholdOverride = Threshold; bHasThresholdOverride = true; }

private:
	bool bHasThresholdOverride = false;
	float SaliencyThresholdOverride = 0.0f;

	/**
	 * Calculate curvature at each point on the trajectory
	 * Curvature k = |dT/ds| where T is the unit tangent vector
	 * @param Positions Array of 3D positions
	 * @param Times Array of times
	 * @return Array of curvature values
	 */
	TArray<float> CalculateCurvature(
		const TArray<FVector>& Positions,
		const TArray<float>& Times);

	/**
	 * Calculate the discrete curvature at a single point using Menger curvature
	 * k = 4*Area(P0,P1,P2) / (|P0-P1| * |P1-P2| * |P2-P0|)
	 */
	float CalculatePointCurvature(const FVector& P0, const FVector& P1, const FVector& P2);

	/**
	 * Find salient points where curvature changes rapidly
	 * @param Curvatures Array of curvature values
	 * @param Times Array of times
	 * @param WindowSize Size of analysis window in seconds
	 * @param Threshold Saliency threshold for detection
	 * @return Indices of salient points
	 */
	TArray<int32> FindSalientPoints(
		const TArray<float>& Curvatures,
		const TArray<float>& Times,
		float WindowSize,
		float Threshold);

	/**
	 * Determine if a salient point represents a foot contact (vs lift-off)
	 * based on the height change direction
	 */
	bool IsFootContact(
		const TArray<FVector>& Positions,
		int32 SalientIndex);
};

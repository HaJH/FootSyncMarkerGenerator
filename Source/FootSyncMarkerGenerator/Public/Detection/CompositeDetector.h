// Copyright 2025 Jeonghyeon Ha. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IFootContactDetector.h"
#include "PelvisCrossingDetector.h"
#include "VelocityCurveDetector.h"
#include "SaliencyDetector.h"

/**
 * Combines multiple detection methods using weighted voting
 * Results from each detector are clustered by time and merged
 */
class FOOTSYNCMARKERGENERATOR_API FCompositeDetector : public IFootContactDetector
{
public:
	FCompositeDetector();
	explicit FCompositeDetector(const struct FCompositeDetectionWeights& Weights);
	virtual ~FCompositeDetector() = default;

	// IFootContactDetector interface
	virtual TArray<FFootContactResult> DetectContacts(
		UAnimSequence* AnimSequence,
		const FSyncFootDefinition& Foot,
		const FLocomotionPreset& Preset) override;

	virtual FString GetDetectorName() const override { return TEXT("Composite"); }

	virtual void SetVelocityThreshold(float Threshold) override;
	virtual void SetSaliencyThreshold(float Threshold) override;

private:
	/**
	 * Merge results from multiple detectors using time-based clustering
	 */
	TArray<FFootContactResult> MergeResults(
		const TArray<FFootContactResult>& PelvisResults,
		const TArray<FFootContactResult>& VelocityResults,
		const TArray<FFootContactResult>& SaliencyResults);

	/**
	 * Cluster results by time proximity
	 * Results within MergeThreshold are grouped together
	 */
	TArray<TArray<FFootContactResult>> ClusterResultsByTime(
		const TArray<FFootContactResult>& AllResults,
		float MergeThreshold);

	/**
	 * Calculate final result from a cluster of nearby detections
	 */
	FFootContactResult CalculateClusterResult(const TArray<FFootContactResult>& Cluster);

	/**
	 * Get the weight for a detection method
	 */
	float GetWeightForMethod(EFootContactDetectionMethod Method) const;

	// Individual detectors
	TUniquePtr<FPelvisCrossingDetector> PelvisDetector;
	TUniquePtr<FVelocityCurveDetector> VelocityDetector;
	TUniquePtr<FSaliencyDetector> SaliencyDetector;

	// Weights for each method
	float PelvisCrossingWeight;
	float VelocityCurveWeight;
	float SaliencyWeight;
};

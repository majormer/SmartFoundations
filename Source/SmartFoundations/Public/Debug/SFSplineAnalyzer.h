// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildableConveyorBelt.h"

/**
 * Debug utility to analyze vanilla Satisfactory pipe and belt splines
 * Used to reverse-engineer spline tangent formulas
 */
class SMARTFOUNDATIONS_API FSFSplineAnalyzer
{
public:
	/**
	 * Analyze all pipes and belts within radius of a location
	 * Extracts spline data and calculates tangent ratios
	 * 
	 * @param World - World context
	 * @param Location - Center point to search from
	 * @param Radius - Search radius in cm (default 5000cm = 50m)
	 */
	static void AnalyzeNearLocation(UWorld* World, const FVector& Location, float Radius = 5000.0f);

	/**
	 * Analyze all pipes within radius of a location
	 * 
	 * @param World - World context
	 * @param Location - Center point to search from
	 * @param Radius - Search radius in cm (default 5000cm = 50m)
	 */
	static void AnalyzePipesNearLocation(UWorld* World, const FVector& Location, float Radius = 5000.0f);

	/**
	 * Analyze all belts within radius of a location
	 * 
	 * @param World - World context
	 * @param Location - Center point to search from
	 * @param Radius - Search radius in cm (default 5000cm = 50m)
	 */
	static void AnalyzeBeltsNearLocation(UWorld* World, const FVector& Location, float Radius = 5000.0f);

	/**
	 * Analyze a specific pipe's spline data
	 * 
	 * @param Pipeline - Pipe to analyze
	 * @return True if analysis succeeded
	 */
	static bool AnalyzePipeSpline(AFGBuildablePipeline* Pipeline);

	/**
	 * Analyze a specific belt's spline data
	 * 
	 * @param Belt - Belt to analyze
	 * @return True if analysis succeeded
	 */
	static bool AnalyzeBeltSpline(AFGBuildableConveyorBelt* Belt);

	/**
	 * Calculate tangent scale factor from spline data
	 * Returns the ratio: TangentLength / Distance
	 * 
	 * @param StartPos - Start point location
	 * @param EndPos - End point location
	 * @param StartTangent - Start point tangent vector
	 * @param EndTangent - End point tangent vector
	 * @return Average tangent scale factor (0.0 to 1.0+)
	 */
	static float CalculateTangentScaleFactor(
		const FVector& StartPos,
		const FVector& EndPos,
		const FVector& StartTangent,
		const FVector& EndTangent
	);
};

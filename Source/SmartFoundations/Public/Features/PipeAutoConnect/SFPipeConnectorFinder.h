// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"

class AFGHologram;
class AFGBuildable;
class UFGPipeConnectionComponent;

/**
 * FSFPipeConnectorFinder
 *
 * Helper for detecting pipeline junction holograms and discovering nearby
 * building pipe connectors (production, power, fluid buffers).
 */
class SMARTFOUNDATIONS_API FSFPipeConnectorFinder
{
public:
	/** Check if a hologram is a pipeline junction hologram that Smart should handle. */
	static bool IsPipelineJunctionHologram(AFGHologram* Hologram);

	/** Find nearby candidate buildings with pipe connections around a junction hologram. */
	static void FindNearbyPipeBuildings(
		AFGHologram* JunctionHologram,
		float SearchRadius,
		TArray<AFGBuildable*>& OutBuildings);

	/** Get all unconnected pipe connectors for a given building. */
	static void GetPipeConnectors(
		AFGBuildable* Building,
		TArray<UFGPipeConnectionComponent*>& OutConnectors);

	/** Ports which native attachment construction will consume before Auto-Connect. */
	static TArray<FName> GetNativeSnapReservations(AFGHologram* JunctionHologram);

	/** Get junction ports available to Auto-Connect, excluding native snap reservations. */
	static void GetJunctionConnectors(
		AFGHologram* JunctionHologram,
		TArray<UFGPipeConnectionComponent*>& OutConnectors);
	
	/**
	 * Check if a pipe connection angle is valid (< 30 degrees from connector forward).
	 * Returns true if the angle between the connection direction and connector forward is acceptable.
	 */
	static bool IsConnectionAngleValid(
		UFGPipeConnectionComponent* Connector,
		const FVector& ConnectionDirection,
		float MaxAngleDegrees = 30.0f);
};

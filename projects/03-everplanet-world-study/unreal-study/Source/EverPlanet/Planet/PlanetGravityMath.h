// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Pure spherical-gravity math shared by runtime code and automation tests.
 *
 * Keeping these operations independent from Actors and Components makes the
 * coordinate rules deterministic and inexpensive to verify.
 */
struct EVERPLANET_API FPlanetGravityMath
{
	/** Returns the normalized direction from the planet center to the world location. */
	static bool TryGetSurfaceNormal(
		const FVector& WorldLocation,
		const FVector& PlanetCenter,
		FVector& OutSurfaceNormal);

	/** Returns the normalized direction from the world location toward the planet center. */
	static bool TryGetGravityDirection(
		const FVector& WorldLocation,
		const FVector& PlanetCenter,
		FVector& OutGravityDirection);

	/** Projects a direction onto the plane whose normal is SurfaceNormal. */
	static FVector ProjectOntoTangentPlane(
		const FVector& Direction,
		const FVector& SurfaceNormal);

	/**
	 * Builds a normalized right-handed Forward/Right/Up basis on a planet surface.
	 * A deterministic fallback is used when PreferredForward is parallel to Up.
	 */
	static bool TryBuildTangentBasis(
		const FVector& PreferredForward,
		const FVector& SurfaceNormal,
		FVector& OutForward,
		FVector& OutRight);
};

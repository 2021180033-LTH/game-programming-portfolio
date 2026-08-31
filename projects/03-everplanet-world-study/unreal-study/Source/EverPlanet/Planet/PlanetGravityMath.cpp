// Copyright Epic Games, Inc. All Rights Reserved.

#include "Planet/PlanetGravityMath.h"

bool FPlanetGravityMath::TryGetSurfaceNormal(
	const FVector& WorldLocation,
	const FVector& PlanetCenter,
	FVector& OutSurfaceNormal)
{
	OutSurfaceNormal = WorldLocation - PlanetCenter;
	if (OutSurfaceNormal.ContainsNaN() || !OutSurfaceNormal.Normalize())
	{
		OutSurfaceNormal = FVector::ZeroVector;
		return false;
	}

	return true;
}

bool FPlanetGravityMath::TryGetGravityDirection(
	const FVector& WorldLocation,
	const FVector& PlanetCenter,
	FVector& OutGravityDirection)
{
	OutGravityDirection = PlanetCenter - WorldLocation;
	if (OutGravityDirection.ContainsNaN() || !OutGravityDirection.Normalize())
	{
		OutGravityDirection = FVector::ZeroVector;
		return false;
	}

	return true;
}

FVector FPlanetGravityMath::ProjectOntoTangentPlane(
	const FVector& Direction,
	const FVector& SurfaceNormal)
{
	if (Direction.ContainsNaN() || SurfaceNormal.ContainsNaN())
	{
		return FVector::ZeroVector;
	}

	const FVector SafeSurfaceNormal = SurfaceNormal.GetSafeNormal();
	if (SafeSurfaceNormal.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	return FVector::VectorPlaneProject(Direction, SafeSurfaceNormal);
}

bool FPlanetGravityMath::TryBuildTangentBasis(
	const FVector& PreferredForward,
	const FVector& SurfaceNormal,
	FVector& OutForward,
	FVector& OutRight)
{
	OutForward = FVector::ZeroVector;
	OutRight = FVector::ZeroVector;
	if (PreferredForward.ContainsNaN() || SurfaceNormal.ContainsNaN())
	{
		return false;
	}

	const FVector LocalUp = SurfaceNormal.GetSafeNormal();
	if (LocalUp.IsNearlyZero())
	{
		return false;
	}

	OutForward = ProjectOntoTangentPlane(PreferredForward, LocalUp);
	if (!OutForward.Normalize())
	{
		const FVector FallbackReference =
			FMath::Abs(FVector::DotProduct(LocalUp, FVector::UpVector)) < 0.99
				? FVector::UpVector
				: FVector::ForwardVector;

		OutForward = FVector::CrossProduct(FallbackReference, LocalUp);
		if (!OutForward.Normalize())
		{
			return false;
		}
	}

	OutRight = FVector::CrossProduct(LocalUp, OutForward);
	if (!OutRight.Normalize())
	{
		OutForward = FVector::ZeroVector;
		return false;
	}

	// Remove the last traces of floating-point skew while preserving handedness.
	OutForward = FVector::CrossProduct(OutRight, LocalUp).GetSafeNormal();
	return !OutForward.IsNearlyZero();
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Orthonormal camera frame expressed against a planet's local surface frame.
 * X is the camera/view forward axis, Y is screen right, and Z is screen up.
 */
struct EVERPLANET_API FGlobeCameraFrame
{
	FVector LocalUp = FVector::ZeroVector;
	FVector TangentForward = FVector::ZeroVector;
	FVector ViewForward = FVector::ZeroVector;
	FVector ViewRight = FVector::ZeroVector;
	FVector ViewUp = FVector::ZeroVector;
	FQuat ViewRotation = FQuat::Identity;
};

/** Pure local-up camera math shared by runtime code and automation tests. */
struct EVERPLANET_API FGlobeCameraMath
{
	/**
	 * Parallel-transports a tangent heading between two nearby surface normals.
	 * Antipodal normals are intentionally rejected because their shortest rotation
	 * axis is ambiguous; discontinuous transfers must explicitly reseed heading.
	 */
	static bool TryParallelTransportTangent(
		const FVector& PreviousUp,
		const FVector& CurrentUp,
		const FVector& PreviousTangentForward,
		FVector& OutTransportedTangentForward);

	/**
	 * Builds a roll-stable high-quarter-view frame. DownPitchDegrees is measured
	 * downward from the local tangent plane and must be in [0, 89] degrees.
	 */
	static bool TryBuildViewFrame(
		const FVector& LocalUp,
		const FVector& PreferredTangentForward,
		double DownPitchDegrees,
		FGlobeCameraFrame& OutFrame);
};

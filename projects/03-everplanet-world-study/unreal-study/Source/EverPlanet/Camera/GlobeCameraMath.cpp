// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/GlobeCameraMath.h"

#include "Math/RotationMatrix.h"

#include "Planet/PlanetGravityMath.h"

bool FGlobeCameraMath::TryParallelTransportTangent(
	const FVector& PreviousUp,
	const FVector& CurrentUp,
	const FVector& PreviousTangentForward,
	FVector& OutTransportedTangentForward)
{
	OutTransportedTangentForward = FVector::ZeroVector;
	if (PreviousUp.ContainsNaN()
		|| CurrentUp.ContainsNaN()
		|| PreviousTangentForward.ContainsNaN())
	{
		return false;
	}

	const FVector SafePreviousUp = PreviousUp.GetSafeNormal();
	const FVector SafeCurrentUp = CurrentUp.GetSafeNormal();
	const FVector SafePreviousTangent =
		FPlanetGravityMath::ProjectOntoTangentPlane(
			PreviousTangentForward,
			SafePreviousUp).GetSafeNormal();
	if (SafePreviousUp.IsNearlyZero()
		|| SafeCurrentUp.IsNearlyZero()
		|| SafePreviousTangent.IsNearlyZero())
	{
		return false;
	}

	const double UpDot = FMath::Clamp(
		static_cast<double>(FVector::DotProduct(SafePreviousUp, SafeCurrentUp)),
		-1.0,
		1.0);
	if (UpDot <= -1.0 + UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FQuat TransportRotation = FQuat::FindBetweenNormals(
		SafePreviousUp,
		SafeCurrentUp);
	OutTransportedTangentForward =
		FPlanetGravityMath::ProjectOntoTangentPlane(
			TransportRotation.RotateVector(SafePreviousTangent),
			SafeCurrentUp).GetSafeNormal();
	return !OutTransportedTangentForward.IsNearlyZero()
		&& !OutTransportedTangentForward.ContainsNaN();
}

bool FGlobeCameraMath::TryBuildViewFrame(
	const FVector& LocalUp,
	const FVector& PreferredTangentForward,
	const double DownPitchDegrees,
	FGlobeCameraFrame& OutFrame)
{
	OutFrame = FGlobeCameraFrame();
	if (LocalUp.ContainsNaN()
		|| PreferredTangentForward.ContainsNaN()
		|| !FMath::IsFinite(DownPitchDegrees)
		|| DownPitchDegrees < 0.0
		|| DownPitchDegrees > 89.0)
	{
		return false;
	}

	OutFrame.LocalUp = LocalUp.GetSafeNormal();
	FVector TangentRight = FVector::ZeroVector;
	if (!FPlanetGravityMath::TryBuildTangentBasis(
			PreferredTangentForward,
			OutFrame.LocalUp,
			OutFrame.TangentForward,
			TangentRight))
	{
		OutFrame = FGlobeCameraFrame();
		return false;
	}

	const double PitchRadians = FMath::DegreesToRadians(DownPitchDegrees);
	OutFrame.ViewForward = (
		OutFrame.TangentForward * FMath::Cos(PitchRadians)
		- OutFrame.LocalUp * FMath::Sin(PitchRadians)).GetSafeNormal();
	OutFrame.ViewRight = TangentRight;
	OutFrame.ViewUp = FVector::CrossProduct(
		OutFrame.ViewForward,
		OutFrame.ViewRight).GetSafeNormal();
	if (OutFrame.ViewForward.IsNearlyZero()
		|| OutFrame.ViewRight.IsNearlyZero()
		|| OutFrame.ViewUp.IsNearlyZero())
	{
		OutFrame = FGlobeCameraFrame();
		return false;
	}

	OutFrame.ViewRotation = FRotationMatrix::MakeFromXY(
		OutFrame.ViewForward,
		OutFrame.ViewRight).ToQuat().GetNormalized();
	return !OutFrame.ViewRotation.ContainsNaN();
}

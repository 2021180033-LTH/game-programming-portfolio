// Copyright Epic Games, Inc. All Rights Reserved.

#include "Planet/EverPlanetSphericalCharacter.h"

#include "Camera/CameraComponent.h"

#include "Planet/PlanetGravityComponent.h"
#include "Planet/PlanetGravityMath.h"

AEverPlanetSphericalCharacter::AEverPlanetSphericalCharacter()
{
	PlanetGravityComponent = CreateDefaultSubobject<UPlanetGravityComponent>(
		TEXT("PlanetGravityComponent"));
}

void AEverPlanetSphericalCharacter::ResetPlanetMovementHeading(
	FVector PreferredTangentForward)
{
	LastValidTangentForward = FVector::ZeroVector;
	if (PreferredTangentForward.IsNearlyZero() || !PlanetGravityComponent)
	{
		return;
	}

	FVector SurfaceNormal = FVector::ZeroVector;
	if (!PlanetGravityComponent->TryGetSurfaceNormal(SurfaceNormal))
	{
		return;
	}

	LastValidTangentForward = FPlanetGravityMath::ProjectOntoTangentPlane(
		PreferredTangentForward,
		SurfaceNormal).GetSafeNormal();
}

void AEverPlanetSphericalCharacter::DoMove(float Right, float Forward)
{
	FVector SurfaceNormal;
	if (GetController()
		&& PlanetGravityComponent
		&& PlanetGravityComponent->TryGetSurfaceNormal(SurfaceNormal))
	{
		const UCameraComponent* FollowCameraComponent = GetFollowCamera();
		FVector PreferredForward = FollowCameraComponent
			? FollowCameraComponent->GetForwardVector()
			: GetActorForwardVector();
		const FVector ProjectedCameraForward =
			FPlanetGravityMath::ProjectOntoTangentPlane(PreferredForward, SurfaceNormal);
		if (ProjectedCameraForward.SizeSquared() <= UE_KINDA_SMALL_NUMBER
			&& !LastValidTangentForward.IsNearlyZero())
		{
			PreferredForward = LastValidTangentForward;
		}

		FVector TangentForward;
		FVector TangentRight;
		if (FPlanetGravityMath::TryBuildTangentBasis(
			PreferredForward,
			SurfaceNormal,
			TangentForward,
			TangentRight))
		{
			// A camera direction that crosses the radial singularity has two equally
			// valid tangent signs. Keep the sign closest to the previous heading so
			// forward input does not snap by 180 degrees at that boundary.
			if (!LastValidTangentForward.IsNearlyZero()
				&& FVector::DotProduct(TangentForward, LastValidTangentForward) < 0.0)
			{
				TangentForward *= -1.0;
				TangentRight *= -1.0;
			}

			LastValidTangentForward = TangentForward;
			AddMovementInput(TangentForward, Forward);
			AddMovementInput(TangentRight, Right);
			return;
		}
	}

	LastValidTangentForward = FVector::ZeroVector;
	Super::DoMove(Right, Forward);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/EverPlanetGlobeCameraCharacter.h"

#include "Camera/PlanetGlobeCameraComponent.h"

AEverPlanetGlobeCameraCharacter::AEverPlanetGlobeCameraCharacter()
{
	PlanetGlobeCameraComponent = CreateDefaultSubobject<UPlanetGlobeCameraComponent>(
		TEXT("PlanetGlobeCameraComponent"));
}

void AEverPlanetGlobeCameraCharacter::DoLook(const float Yaw, const float Pitch)
{
	if (PlanetGlobeCameraComponent)
	{
		PlanetGlobeCameraComponent->AddLookInput(Yaw, Pitch);
	}
}

bool AEverPlanetGlobeCameraCharacter::ResetGlobeCameraHeading(
	FVector PreferredTangentForward,
	const bool bSnapCameraRotation)
{
	ResetPlanetMovementHeading(PreferredTangentForward);
	return PlanetGlobeCameraComponent
		&& PlanetGlobeCameraComponent->ResetCameraFrame(
			PreferredTangentForward,
			bSnapCameraRotation);
}

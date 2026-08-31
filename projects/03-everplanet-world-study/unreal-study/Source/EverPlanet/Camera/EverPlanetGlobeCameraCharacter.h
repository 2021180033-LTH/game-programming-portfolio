// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Planet/EverPlanetSphericalCharacter.h"
#include "EverPlanetGlobeCameraCharacter.generated.h"

class UPlanetGlobeCameraComponent;

/**
 * Spherical character that opts into the M2 local-up globe camera rig.
 * M1 camera defaults and its manual probe remain isolated and unchanged.
 */
UCLASS(Abstract, Blueprintable)
class EVERPLANET_API AEverPlanetGlobeCameraCharacter : public AEverPlanetSphericalCharacter
{
	GENERATED_BODY()

public:
	AEverPlanetGlobeCameraCharacter();

	virtual void DoLook(float Yaw, float Pitch) override;

	/** Reseeds movement and camera heading after a teleport or planet transfer. */
	UFUNCTION(BlueprintCallable, Category = "Globe Camera")
	bool ResetGlobeCameraHeading(
		FVector PreferredTangentForward,
		bool bSnapCameraRotation = true);

	UFUNCTION(BlueprintPure, Category = "Globe Camera")
	UPlanetGlobeCameraComponent* GetPlanetGlobeCameraComponent() const
	{
		return PlanetGlobeCameraComponent;
	}

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlanetGlobeCameraComponent> PlanetGlobeCameraComponent;
};

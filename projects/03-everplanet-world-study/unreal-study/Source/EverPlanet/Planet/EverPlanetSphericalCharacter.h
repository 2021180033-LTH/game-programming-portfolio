// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EverPlanetCharacter.h"
#include "EverPlanetSphericalCharacter.generated.h"

class UPlanetGravityComponent;

/**
 * Third Person character variant that opts into planet-relative gravity and
 * camera-projected tangent movement. The template character remains unchanged.
 */
UCLASS(Abstract, Blueprintable)
class EVERPLANET_API AEverPlanetSphericalCharacter : public AEverPlanetCharacter
{
	GENERATED_BODY()

public:
	AEverPlanetSphericalCharacter();

	virtual void DoMove(float Right, float Forward) override;

	/**
	 * Clears or seeds the tangent-heading continuity state after a teleport or
	 * other discontinuous surface transfer. PreferredTangentForward is projected
	 * onto the current planet tangent plane before it is stored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Planet Gravity")
	void ResetPlanetMovementHeading(FVector PreferredTangentForward);

	UFUNCTION(BlueprintPure, Category = "Planet Gravity")
	UPlanetGravityComponent* GetPlanetGravityComponent() const { return PlanetGravityComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlanetGravityComponent> PlanetGravityComponent;

	/** Preserves heading when the camera forward becomes radial and cannot define a tangent. */
	UPROPERTY(Transient)
	FVector LastValidTangentForward = FVector::ZeroVector;
};

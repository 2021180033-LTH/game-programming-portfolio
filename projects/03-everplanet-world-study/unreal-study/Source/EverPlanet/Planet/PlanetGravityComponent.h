// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlanetGravityComponent.generated.h"

class ACharacter;
class APlanetBody;
class UCharacterMovementComponent;

/**
 * Supplies a Character Movement Component with a planet-relative gravity direction.
 * It deliberately delegates walking, falling, floor checks, jumping, and capsule
 * alignment to UE 5.8's built-in arbitrary-gravity path.
 */
UCLASS(ClassGroup = (EverPlanet), meta = (BlueprintSpawnableComponent))
class EVERPLANET_API UPlanetGravityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlanetGravityComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Assigns a fixed source and disables automatic nearest-planet selection. */
	UFUNCTION(BlueprintCallable, Category = "Planet Gravity")
	void SetGravitySource(APlanetBody* NewGravitySource);

	UFUNCTION(BlueprintPure, Category = "Planet Gravity")
	APlanetBody* GetGravitySource() const { return GravitySource; }

	UFUNCTION(BlueprintPure, Category = "Planet Gravity")
	bool TryGetSurfaceNormal(FVector& OutSurfaceNormal) const;

	UFUNCTION(BlueprintPure, Category = "Planet Gravity")
	bool TryGetGravityDirection(FVector& OutGravityDirection) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RefreshGravitySource();
	APlanetBody* FindNearestPlanet() const;
	void ApplyGravityDirection();
	void RestoreDefaultGravityDirection();

	/** Selects a planet at startup and when the current source becomes invalid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Gravity", meta = (AllowPrivateAccess = "true"))
	bool bAutoSelectNearestPlanet = true;

	/** Fixed source when automatic selection is disabled; runtime result when it is enabled. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Planet Gravity", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<APlanetBody> GravitySource;

	/** Retry interval used only while automatic selection has no valid source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Gravity", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float PlanetSearchInterval = 0.5f;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> CharacterOwner;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> CharacterMovement;

	float TimeSincePlanetSearch = 0.0f;
	bool bAppliedPlanetGravity = false;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlanetBody.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/**
 * Minimal spherical planet used by the M1 movement spike.
 * The actor location is the gravity center and SurfaceRadius controls the
 * built-in sphere mesh used for both visualization and collision.
 */
UCLASS(BlueprintType, Blueprintable)
class EVERPLANET_API APlanetBody : public AActor
{
	GENERATED_BODY()

public:
	APlanetBody();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintPure, Category = "Planet")
	FVector GetPlanetCenter() const;

	UFUNCTION(BlueprintPure, Category = "Planet")
	float GetSurfaceRadius() const { return SurfaceRadius; }

	UFUNCTION(BlueprintPure, Category = "Planet")
	bool TryGetSurfaceNormal(const FVector& WorldLocation, FVector& OutSurfaceNormal) const;

	UFUNCTION(BlueprintPure, Category = "Planet")
	bool TryGetGravityDirection(const FVector& WorldLocation, FVector& OutGravityDirection) const;

	UFUNCTION(BlueprintPure, Category = "Planet")
	UStaticMeshComponent* GetPlanetMesh() const { return PlanetMesh; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PlanetMesh;

	/** Radius in Unreal centimeters. The built-in sphere mesh has a 50 cm source radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Planet", meta = (AllowPrivateAccess = "true", ClampMin = "100.0", UIMin = "100.0", Units = "cm"))
	float SurfaceRadius = 3000.0f;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyAuthoritativeScale();
};

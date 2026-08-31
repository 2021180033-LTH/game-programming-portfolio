// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/EverPlanetWorldTypes.h"
#include "EverPlanetRegionDescriptor.generated.h"

class UEverPlanetWorldCatalog;

/** Exactly one descriptor identifies each regional level to the progression subsystem. */
UCLASS()
class EVERPLANET_API AEverPlanetRegionDescriptor : public AActor
{
	GENERATED_BODY()

public:
	AEverPlanetRegionDescriptor();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "EverPlanet|World")
	FEverPlanetRegionKey GetRegionKey() const;

	UFUNCTION(BlueprintPure, Category = "EverPlanet|World")
	UEverPlanetWorldCatalog* GetWorldCatalog() const { return WorldCatalog; }

private:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "EverPlanet|World", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEverPlanetWorldCatalog> WorldCatalog;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "EverPlanet|World", meta = (AllowPrivateAccess = "true"))
	FName PlanetId = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "EverPlanet|World", meta = (AllowPrivateAccess = "true"))
	FName RegionId = NAME_None;
};

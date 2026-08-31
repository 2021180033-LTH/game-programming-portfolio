// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "World/EverPlanetWorldTypes.h"
#include "EverPlanetWorldCatalog.generated.h"

/**
 * Single authority for logical planets, regional levels, spawn anchors, and travel links.
 * Runtime portals reference only their stable local ID plus LinkId; destination data stays here.
 */
UCLASS(BlueprintType)
class EVERPLANET_API UEverPlanetWorldCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Exact fail-closed entry address for a new local profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FEverPlanetWorldAddress InitialAddress;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	TArray<FEverPlanetPlanetDefinition> Planets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	TArray<FEverPlanetRegionDefinition> Regions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	TArray<FEverPlanetTravelLinkDefinition> TravelLinks;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Blueprint/editor-facing validation with a human-readable exact failure. */
	UFUNCTION(BlueprintCallable, Category = "EverPlanet|World")
	bool ValidateCatalog(FString& OutError) const;

	bool ValidateCatalogDetailed(
		EEverPlanetWorldFailure& OutFailure,
		FString& OutError) const;

	const FEverPlanetPlanetDefinition* FindPlanet(FName PlanetId) const;
	const FEverPlanetRegionDefinition* FindRegion(FName RegionId) const;
	const FEverPlanetTravelLinkDefinition* FindTravelLink(FName LinkId) const;

	static bool IsStableId(FName Identifier);
};

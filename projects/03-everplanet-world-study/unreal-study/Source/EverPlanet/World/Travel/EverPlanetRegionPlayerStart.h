// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "EverPlanetRegionPlayerStart.generated.h"

/** Stable-ID player start used by transactional regional travel. */
UCLASS()
class EVERPLANET_API AEverPlanetRegionPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "EverPlanet|World")
	FName GetSpawnAnchorId() const { return SpawnAnchorId; }

private:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "EverPlanet|World", meta = (AllowPrivateAccess = "true"))
	FName SpawnAnchorId = NAME_None;
};

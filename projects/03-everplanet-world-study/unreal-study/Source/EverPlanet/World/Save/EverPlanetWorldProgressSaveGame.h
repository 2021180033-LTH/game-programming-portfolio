// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "World/EverPlanetWorldTypes.h"
#include "EverPlanetWorldProgressSaveGame.generated.h"

/** Versioned local single-player world/quest progress. Actor pointers are never serialized. */
UCLASS()
class EVERPLANET_API UEverPlanetWorldProgressSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSaveVersion = 2;

	UEverPlanetWorldProgressSaveGame();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
	int32 SaveVersion = CurrentSaveVersion;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
	FEverPlanetWorldProgressSnapshot Snapshot;

	/** CRC of a canonical logical snapshot payload. Used to reject a torn journal entry. */
	UPROPERTY(SaveGame)
	uint32 SnapshotChecksum = 0;
};

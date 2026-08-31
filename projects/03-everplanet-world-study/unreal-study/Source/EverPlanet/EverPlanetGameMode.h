// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EverPlanetGameMode.generated.h"

class AEverPlanetRegionDescriptor;
class AEverPlanetRegionPlayerStart;
class AController;
class APawn;

enum class EEverPlanetResolvedArrivalMode : uint8
{
	None,
	Direct,
	Pending
};

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class EVERPLANET_API AEverPlanetGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Constructor */
	AEverPlanetGameMode();

	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void RestartPlayerAtTransform(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual AActor* FindPlayerStart_Implementation(
		AController* Player,
		const FString& IncomingName) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override;

private:
	bool IsRegionalSpawnFlowActive() const;
	void ClearResolvedArrival();

	/**
	 * Opts a GameMode class into the exact descriptor + stable-anchor spawn contract.
	 * The shared native base stays false so legacy and M1/M2 test GameModes remain
	 * non-regional; the Product GameMode asset is required to set this true.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EverPlanet|World", meta = (AllowPrivateAccess = "true"))
	bool bRequireRegionalSpawnFlow = false;

	TWeakObjectPtr<AController> ResolvedController;
	TWeakObjectPtr<AEverPlanetRegionDescriptor> ResolvedDescriptor;
	TWeakObjectPtr<AEverPlanetRegionPlayerStart> ResolvedStart;
	EEverPlanetResolvedArrivalMode ResolvedArrivalMode = EEverPlanetResolvedArrivalMode::None;
};




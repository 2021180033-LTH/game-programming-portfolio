// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/Travel/EverPlanetRegionDescriptor.h"

#include "Engine/GameInstance.h"

#include "EverPlanet.h"
#include "World/Data/EverPlanetWorldCatalog.h"
#include "World/Progression/EverPlanetWorldProgressSubsystem.h"

AEverPlanetRegionDescriptor::AEverPlanetRegionDescriptor()
{
	PrimaryActorTick.bCanEverTick = false;
}

FEverPlanetRegionKey AEverPlanetRegionDescriptor::GetRegionKey() const
{
	FEverPlanetRegionKey Key;
	Key.PlanetId = PlanetId;
	Key.RegionId = RegionId;
	return Key;
}

void AEverPlanetRegionDescriptor::BeginPlay()
{
	Super::BeginPlay();

	UGameInstance* GameInstance = GetGameInstance();
	UEverPlanetWorldProgressSubsystem* ProgressSubsystem = GameInstance
		? GameInstance->GetSubsystem<UEverPlanetWorldProgressSubsystem>()
		: nullptr;
	if (!ProgressSubsystem)
	{
		UE_LOG(
			LogEverPlanet,
			Error,
			TEXT("WORLD_REGION_DESCRIPTOR_FAILED=reason=no-subsystem actor=%s"),
			*GetName());
		return;
	}

	const FEverPlanetRegionKey Key = GetRegionKey();
	if (!ProgressSubsystem->NotifyRegionLoaded(WorldCatalog, Key))
	{
		UE_LOG(
			LogEverPlanet,
			Error,
			TEXT("WORLD_REGION_DESCRIPTOR_FAILED=region=%s reason=%s"),
			*Key.ToString(),
			*ProgressSubsystem->GetLastFailureMessage());
		return;
	}
	UE_LOG(
		LogEverPlanet,
		Log,
		TEXT("WORLD_REGION_DESCRIPTOR_READY=region=%s catalog=%s"),
		*Key.ToString(),
		*GetPathNameSafe(WorldCatalog));
}

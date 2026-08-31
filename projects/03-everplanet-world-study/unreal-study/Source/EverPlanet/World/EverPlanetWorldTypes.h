// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "EverPlanetWorldTypes.generated.h"

class UWorld;

/** Why a world-catalog, region-travel, or arrival operation was rejected. */
UENUM(BlueprintType)
enum class EEverPlanetWorldFailure : uint8
{
	None,
	SystemNotReady,
	InvalidIdentifier,
	DuplicateIdentifier,
	InvalidCatalog,
	UnknownPlanet,
	UnknownRegion,
	UnknownTravelLink,
	WrongSourceRegion,
	Locked,
	TransitionAlreadyPending,
	DestinationMapInvalid,
	DescriptorMismatch,
	MissingSpawnAnchor,
	DuplicateSpawnAnchor,
	SpawnNotResolved,
	SaveLoadFailed,
	SaveWriteFailed
};

/** Whether a travel link stays on one logical planet or changes planets. */
UENUM(BlueprintType)
enum class EEverPlanetTravelScope : uint8
{
	IntraPlanet,
	InterPlanet
};

/** Why a cross-level transition transaction exists. */
UENUM(BlueprintType)
enum class EEverPlanetTransitionKind : uint8
{
	PortalTravel,
	StartupResume
};

/** Stable logical identity for one regional map. */
USTRUCT(BlueprintType)
struct EVERPLANET_API FEverPlanetRegionKey
{
	GENERATED_BODY()

	FEverPlanetRegionKey() = default;

	FEverPlanetRegionKey(const FName InPlanetId, const FName InRegionId)
		: PlanetId(InPlanetId)
		, RegionId(InRegionId)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World")
	FName PlanetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World")
	FName RegionId = NAME_None;

	bool IsValid() const
	{
		return !PlanetId.IsNone() && !RegionId.IsNone();
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%s/%s"), *PlanetId.ToString(), *RegionId.ToString());
	}

	friend bool operator==(const FEverPlanetRegionKey& Left, const FEverPlanetRegionKey& Right)
	{
		return Left.PlanetId == Right.PlanetId && Left.RegionId == Right.RegionId;
	}

	friend bool operator!=(const FEverPlanetRegionKey& Left, const FEverPlanetRegionKey& Right)
	{
		return !(Left == Right);
	}
};

FORCEINLINE uint32 GetTypeHash(const FEverPlanetRegionKey& Key)
{
	return HashCombine(GetTypeHash(Key.PlanetId), GetTypeHash(Key.RegionId));
}

/** A region plus an exact player-spawn anchor inside that region. */
USTRUCT(BlueprintType)
struct EVERPLANET_API FEverPlanetWorldAddress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World")
	FName PlanetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World")
	FName RegionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "World")
	FName SpawnAnchorId = NAME_None;

	bool IsValid() const
	{
		return !PlanetId.IsNone() && !RegionId.IsNone() && !SpawnAnchorId.IsNone();
	}

	FEverPlanetRegionKey GetRegionKey() const
	{
		return FEverPlanetRegionKey(PlanetId, RegionId);
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT("%s/%s@%s"),
			*PlanetId.ToString(),
			*RegionId.ToString(),
			*SpawnAnchorId.ToString());
	}

	friend bool operator==(const FEverPlanetWorldAddress& Left, const FEverPlanetWorldAddress& Right)
	{
		return Left.PlanetId == Right.PlanetId
			&& Left.RegionId == Right.RegionId
			&& Left.SpawnAnchorId == Right.SpawnAnchorId;
	}

	friend bool operator!=(const FEverPlanetWorldAddress& Left, const FEverPlanetWorldAddress& Right)
	{
		return !(Left == Right);
	}
};

/** One logical planet and the regions that belong to it. */
USTRUCT(BlueprintType)
struct EVERPLANET_API FEverPlanetPlanetDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FName PlanetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FName DefaultRegionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	TArray<FName> RegionIds;
};

/** One regional Unreal level owned by a logical planet. */
USTRUCT(BlueprintType)
struct EVERPLANET_API FEverPlanetRegionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FName PlanetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FName RegionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	TSoftObjectPtr<UWorld> Map;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FName DefaultSpawnAnchorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	TArray<FName> SpawnAnchorIds;

	FEverPlanetRegionKey GetKey() const
	{
		return FEverPlanetRegionKey(PlanetId, RegionId);
	}
};

/** A catalog-owned route. Portals store identity/LinkId only so destination data has one authority. */
USTRUCT(BlueprintType)
struct EVERPLANET_API FEverPlanetTravelLinkDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FName LinkId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	EEverPlanetTravelScope Scope = EEverPlanetTravelScope::IntraPlanet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FEverPlanetRegionKey Source;

	/** Stable identity of the exact portal/interaction point inside Source. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FName SourcePortalId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FEverPlanetWorldAddress Destination;

	/** Explicit policy for links that are usable on a new profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	bool bInitiallyUnlocked = false;

	/** Optional quest condition that unlocks a link which is not initially unlocked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FName RequiredQuestFlag = NAME_None;

	/** Optional flag granted only after the destination descriptor and spawn are committed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World")
	FName ArrivalQuestFlag = NAME_None;
};

/** Local single-player state that survives OpenLevel and is serialized to SaveGame. */
USTRUCT(BlueprintType)
struct EVERPLANET_API FEverPlanetWorldProgressSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
	FEverPlanetWorldAddress CurrentAddress;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
	TArray<FName> CompletedQuestFlags;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
	TArray<FName> UnlockedTravelLinkIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "World")
	int32 Revision = 0;

	bool HasQuestFlag(const FName QuestFlag) const
	{
		return !QuestFlag.IsNone() && CompletedQuestFlags.Contains(QuestFlag);
	}

	bool IsTravelLinkExplicitlyUnlocked(const FName LinkId) const
	{
		return UnlockedTravelLinkIds.Contains(LinkId);
	}
};

/** Non-persistent transaction state kept by the GameInstance subsystem across OpenLevel. */
USTRUCT(BlueprintType)
struct EVERPLANET_API FEverPlanetPendingTransition
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World")
	EEverPlanetTransitionKind Kind = EEverPlanetTransitionKind::PortalTravel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World")
	FName LinkId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World")
	FEverPlanetWorldAddress SourceAddress;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World")
	FEverPlanetWorldAddress DestinationAddress;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World")
	TSoftObjectPtr<UWorld> DestinationMap;

	/** Actual package being left; distinguishes a still-alive departure world from a bad load. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World")
	FName DepartureMapPackageName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World")
	FName ArrivalQuestFlag = NAME_None;
};

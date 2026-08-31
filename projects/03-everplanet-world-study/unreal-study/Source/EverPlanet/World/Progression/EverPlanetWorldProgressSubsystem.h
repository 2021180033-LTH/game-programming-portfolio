// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "World/EverPlanetWorldTypes.h"
#include "EverPlanetWorldProgressSubsystem.generated.h"

class UEverPlanetWorldCatalog;
class UEverPlanetWorldProgressSaveGame;
class UWorld;

/**
 * Game-instance lifetime owner for committed progress and the two-phase OpenLevel transaction.
 * The committed address never changes until the loaded descriptor and exact spawn anchor agree.
 */
UCLASS()
class EVERPLANET_API UEverPlanetWorldProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "EverPlanet|World")
	bool IsProgressReady() const { return bProgressReady; }

	UFUNCTION(BlueprintPure, Category = "EverPlanet|World")
	bool IsTransitionPending() const { return bTransitionPending; }

	UFUNCTION(BlueprintPure, Category = "EverPlanet|World")
	FEverPlanetWorldProgressSnapshot GetProgressSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "EverPlanet|World")
	FEverPlanetPendingTransition GetPendingTransition() const { return PendingTransition; }

	UFUNCTION(BlueprintPure, Category = "EverPlanet|World")
	EEverPlanetWorldFailure GetLastFailure() const { return LastFailure; }

	UFUNCTION(BlueprintPure, Category = "EverPlanet|World")
	FString GetLastFailureMessage() const { return LastFailureMessage; }

	UFUNCTION(BlueprintPure, Category = "EverPlanet|World")
	bool HasQuestFlag(FName QuestFlag) const;

	UFUNCTION(BlueprintCallable, Category = "EverPlanet|World")
	bool SetQuestFlag(FName QuestFlag);

	UFUNCTION(BlueprintCallable, Category = "EverPlanet|World")
	bool UnlockTravelLink(FName LinkId);

	/** Called by a portal after overlap/interaction. The portal owns no destination data. */
	UFUNCTION(BlueprintCallable, Category = "EverPlanet|World")
	bool RequestTravelByLinkId(FName LinkId, FName SourcePortalId);

	/** Called by the one-per-map region descriptor during BeginPlay. */
	bool NotifyRegionLoaded(
		UEverPlanetWorldCatalog* Catalog,
		const FEverPlanetRegionKey& LoadedRegion);

	/** Called by GameMode only after it finds exactly one matching regional PlayerStart. */
	bool ResolvePendingSpawn(
		UEverPlanetWorldCatalog* Catalog,
		const FEverPlanetRegionKey& LoadedRegion,
		FName SpawnAnchorId);

	/** Resolves the exact initial/resume spawn without mutating committed progress. */
	bool ResolveDirectSpawn(
		UEverPlanetWorldCatalog* Catalog,
		const FEverPlanetRegionKey& LoadedRegion,
		FName& OutSpawnAnchorId);

	/** Called by GameMode only after the pawn was actually spawned at the resolved anchor. */
	bool CommitPendingTravel(
		const FEverPlanetRegionKey& LoadedRegion,
		FName SpawnAnchorId);

	/** Commits only a brand-new profile. Existing resumes are idempotent. */
	bool CommitDirectArrival(
		const FEverPlanetRegionKey& LoadedRegion,
		FName SpawnAnchorId);

	/** Records an arrival failure without mutating committed progress. */
	void RejectPendingArrival(EEverPlanetWorldFailure Failure, const FString& Message);

	/** Records a non-transactional regional arrival failure. */
	void RejectDirectArrival(EEverPlanetWorldFailure Failure, const FString& Message);

private:
	static const FString LegacySaveSlotName;
	static const FString JournalSaveSlotA;
	static const FString JournalSaveSlotB;
	static constexpr int32 SaveUserIndex = 0;

	bool LoadProgress();
	bool SaveProgress();
	bool TryLoadJournalSlot(
		const FString& SlotName,
		TObjectPtr<UEverPlanetWorldProgressSaveGame>& OutSave,
		FString& OutError) const;
	bool TryLoadLegacySlot(
		TObjectPtr<UEverPlanetWorldProgressSaveGame>& OutSave,
		FString& OutError) const;
	bool ValidateJournalEntry(
		const UEverPlanetWorldProgressSaveGame* Save,
		FString& OutError) const;
	uint32 ComputeSnapshotChecksum(const FEverPlanetWorldProgressSnapshot& Snapshot) const;
	bool ValidateSnapshotSyntax(const FEverPlanetWorldProgressSnapshot& Snapshot, FString& OutError) const;
	bool ValidateLoadedRegion(
		UEverPlanetWorldCatalog* Catalog,
		const FEverPlanetRegionKey& LoadedRegion,
		EEverPlanetWorldFailure& OutFailure,
		FString& OutError) const;
	bool BeginResumeTransition(
		UEverPlanetWorldCatalog* Catalog,
		const FEverPlanetWorldAddress& SavedAddress);
	bool IsMapPackageAvailable(
		const TSoftObjectPtr<UWorld>& Map,
		FString& OutError) const;
	void ClearPendingTransition();
	void HandleTravelFailure(
		UWorld* FailedWorld,
		ETravelFailure::Type FailureType,
		const FString& ErrorString);
	void SetFailure(EEverPlanetWorldFailure Failure, const FString& Message);
	void ClearFailure();

	UPROPERTY(Transient)
	TObjectPtr<UEverPlanetWorldProgressSaveGame> ProgressSave;

	UPROPERTY(Transient)
	TObjectPtr<UEverPlanetWorldCatalog> ActiveCatalog;

	UPROPERTY(Transient)
	FEverPlanetPendingTransition PendingTransition;

	UPROPERTY(Transient)
	bool bProgressReady = false;

	UPROPERTY(Transient)
	bool bTransitionPending = false;

	UPROPERTY(Transient)
	bool bPendingSpawnResolved = false;

	UPROPERTY(Transient)
	FName ResolvedSpawnAnchorId = NAME_None;

	UPROPERTY(Transient)
	EEverPlanetWorldFailure LastFailure = EEverPlanetWorldFailure::None;

	UPROPERTY(Transient)
	FString LastFailureMessage;

	FString ActiveJournalSlotName;
	FDelegateHandle TravelFailureHandle;
};

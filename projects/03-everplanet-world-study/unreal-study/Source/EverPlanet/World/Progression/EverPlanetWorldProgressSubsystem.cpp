// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/Progression/EverPlanetWorldProgressSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"

#include "EverPlanet.h"
#include "World/Data/EverPlanetWorldCatalog.h"
#include "World/Save/EverPlanetWorldProgressSaveGame.h"

namespace EverPlanetWorldProgress
{
	FString JoinStableIds(const TArray<FName>& Identifiers)
	{
		TArray<FString> Values;
		Values.Reserve(Identifiers.Num());
		for (const FName Identifier : Identifiers)
		{
			Values.Add(Identifier.ToString());
		}
		Values.Sort();
		return FString::Join(Values, TEXT(","));
	}

	FString BuildCanonicalSnapshotPayload(const FEverPlanetWorldProgressSnapshot& Snapshot)
	{
		return FString::Printf(
			TEXT("v=%d|rev=%d|address=%s|quests=%s|links=%s"),
			UEverPlanetWorldProgressSaveGame::CurrentSaveVersion,
			Snapshot.Revision,
			*Snapshot.CurrentAddress.ToString(),
			*JoinStableIds(Snapshot.CompletedQuestFlags),
			*JoinStableIds(Snapshot.UnlockedTravelLinkIds));
	}
}

const FString UEverPlanetWorldProgressSubsystem::LegacySaveSlotName = TEXT("EverPlanetWorldProgress");
const FString UEverPlanetWorldProgressSubsystem::JournalSaveSlotA = TEXT("EverPlanetWorldProgress_A");
const FString UEverPlanetWorldProgressSubsystem::JournalSaveSlotB = TEXT("EverPlanetWorldProgress_B");

void UEverPlanetWorldProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bProgressReady = LoadProgress();
	if (bProgressReady
		&& ProgressSave
		&& ProgressSave->SaveVersion == 1)
	{
		if (SaveProgress())
		{
			UE_LOG(
				LogEverPlanet,
				Log,
				TEXT("WORLD_PROGRESS_LEGACY_MIGRATED=1 journal=%s revision=%d"),
				*ActiveJournalSlotName,
				ProgressSave->Snapshot.Revision);
		}
		else
		{
			bProgressReady = false;
		}
	}
	if (GEngine)
	{
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(
			this,
			&UEverPlanetWorldProgressSubsystem::HandleTravelFailure);
	}
	if (bProgressReady)
	{
		UE_LOG(
			LogEverPlanet,
			Log,
			TEXT("WORLD_PROGRESS_READY=1 slot=%s revision=%d current=%s"),
			ActiveJournalSlotName.IsEmpty() ? TEXT("new-or-legacy") : *ActiveJournalSlotName,
			ProgressSave->Snapshot.Revision,
			*ProgressSave->Snapshot.CurrentAddress.ToString());
	}
}

void UEverPlanetWorldProgressSubsystem::Deinitialize()
{
	if (GEngine && TravelFailureHandle.IsValid())
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	TravelFailureHandle.Reset();
	ActiveCatalog = nullptr;
	ProgressSave = nullptr;
	bProgressReady = false;
	ClearPendingTransition();
	ActiveJournalSlotName.Reset();
	Super::Deinitialize();
}

FEverPlanetWorldProgressSnapshot UEverPlanetWorldProgressSubsystem::GetProgressSnapshot() const
{
	return ProgressSave ? ProgressSave->Snapshot : FEverPlanetWorldProgressSnapshot();
}

bool UEverPlanetWorldProgressSubsystem::HasQuestFlag(const FName QuestFlag) const
{
	return bProgressReady && ProgressSave && ProgressSave->Snapshot.HasQuestFlag(QuestFlag);
}

bool UEverPlanetWorldProgressSubsystem::ValidateSnapshotSyntax(
	const FEverPlanetWorldProgressSnapshot& Snapshot,
	FString& OutError) const
{
	OutError.Reset();
	if (Snapshot.Revision < 0)
	{
		OutError = TEXT("World progress revision cannot be negative.");
		return false;
	}

	const FEverPlanetWorldAddress& Current = Snapshot.CurrentAddress;
	const bool bAddressEmpty = Current.PlanetId.IsNone()
		&& Current.RegionId.IsNone()
		&& Current.SpawnAnchorId.IsNone();
	if (!bAddressEmpty
		&& (!Current.IsValid()
			|| !UEverPlanetWorldCatalog::IsStableId(Current.PlanetId)
			|| !UEverPlanetWorldCatalog::IsStableId(Current.RegionId)
			|| !UEverPlanetWorldCatalog::IsStableId(Current.SpawnAnchorId)))
	{
		OutError = TEXT("Saved current world address is partial or contains an invalid stable ID.");
		return false;
	}

	TSet<FName> UniqueQuestFlags;
	for (const FName QuestFlag : Snapshot.CompletedQuestFlags)
	{
		if (!UEverPlanetWorldCatalog::IsStableId(QuestFlag) || UniqueQuestFlags.Contains(QuestFlag))
		{
			OutError = FString::Printf(
				TEXT("Saved quest flag is invalid or duplicated: %s"),
				*QuestFlag.ToString());
			return false;
		}
		UniqueQuestFlags.Add(QuestFlag);
	}

	TSet<FName> UniqueLinkIds;
	for (const FName LinkId : Snapshot.UnlockedTravelLinkIds)
	{
		if (!UEverPlanetWorldCatalog::IsStableId(LinkId) || UniqueLinkIds.Contains(LinkId))
		{
			OutError = FString::Printf(
				TEXT("Saved unlocked LinkId is invalid or duplicated: %s"),
				*LinkId.ToString());
			return false;
		}
		UniqueLinkIds.Add(LinkId);
	}
	return true;
}

bool UEverPlanetWorldProgressSubsystem::LoadProgress()
{
	ClearFailure();
	ActiveJournalSlotName.Reset();

	struct FJournalCandidate
	{
		FString SlotName;
		TObjectPtr<UEverPlanetWorldProgressSaveGame> Save;
	};
	TArray<FJournalCandidate> Candidates;
	FString JournalErrorA;
	FString JournalErrorB;
	const bool bJournalAExists = UGameplayStatics::DoesSaveGameExist(JournalSaveSlotA, SaveUserIndex);
	const bool bJournalBExists = UGameplayStatics::DoesSaveGameExist(JournalSaveSlotB, SaveUserIndex);
	if (bJournalAExists)
	{
		FJournalCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.SlotName = JournalSaveSlotA;
		if (!TryLoadJournalSlot(JournalSaveSlotA, Candidate.Save, JournalErrorA))
		{
			Candidates.Pop();
		}
	}
	if (bJournalBExists)
	{
		FJournalCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.SlotName = JournalSaveSlotB;
		if (!TryLoadJournalSlot(JournalSaveSlotB, Candidate.Save, JournalErrorB))
		{
			Candidates.Pop();
		}
	}

	if (!Candidates.IsEmpty())
	{
		if (Candidates.Num() == 2
			&& Candidates[0].Save->Snapshot.Revision == Candidates[1].Save->Snapshot.Revision)
		{
			const FString PayloadA = EverPlanetWorldProgress::BuildCanonicalSnapshotPayload(
				Candidates[0].Save->Snapshot);
			const FString PayloadB = EverPlanetWorldProgress::BuildCanonicalSnapshotPayload(
				Candidates[1].Save->Snapshot);
			if (Candidates[0].Save->SnapshotChecksum != Candidates[1].Save->SnapshotChecksum
				|| PayloadA != PayloadB)
			{
				SetFailure(
					EEverPlanetWorldFailure::SaveLoadFailed,
					FString::Printf(
						TEXT("World-progress journals disagree at revision %d. A checksum=%u B checksum=%u."),
						Candidates[0].Save->Snapshot.Revision,
						Candidates[0].Save->SnapshotChecksum,
						Candidates[1].Save->SnapshotChecksum));
				return false;
			}
		}

		Candidates.Sort(
			[](const FJournalCandidate& Left, const FJournalCandidate& Right)
			{
				if (Left.Save->Snapshot.Revision != Right.Save->Snapshot.Revision)
				{
					return Left.Save->Snapshot.Revision > Right.Save->Snapshot.Revision;
				}
				return Left.SlotName < Right.SlotName;
			});
		ProgressSave = Candidates[0].Save;
		ActiveJournalSlotName = Candidates[0].SlotName;
		return true;
	}

	if (bJournalAExists || bJournalBExists)
	{
		SetFailure(
			EEverPlanetWorldFailure::SaveLoadFailed,
			FString::Printf(
				TEXT("No valid world-progress journal remains. A=%s B=%s"),
				JournalErrorA.IsEmpty() ? TEXT("missing") : *JournalErrorA,
				JournalErrorB.IsEmpty() ? TEXT("missing") : *JournalErrorB));
		return false;
	}

	const bool bLegacyExists = UGameplayStatics::DoesSaveGameExist(LegacySaveSlotName, SaveUserIndex);
	if (bLegacyExists)
	{
		FString LegacyError;
		if (TryLoadLegacySlot(ProgressSave, LegacyError))
		{
			UE_LOG(
				LogEverPlanet,
				Log,
				TEXT("WORLD_PROGRESS_LEGACY_LOADED=1 migration=initialize revision=%d"),
				ProgressSave->Snapshot.Revision);
			return true;
		}
		SetFailure(EEverPlanetWorldFailure::SaveLoadFailed, LegacyError);
		return false;
	}

	ProgressSave = Cast<UEverPlanetWorldProgressSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UEverPlanetWorldProgressSaveGame::StaticClass()));
	if (!ProgressSave)
	{
		SetFailure(
			EEverPlanetWorldFailure::SaveLoadFailed,
			TEXT("Could not create the initial world-progress SaveGame object."));
		return false;
	}
	return true;
}

bool UEverPlanetWorldProgressSubsystem::SaveProgress()
{
	if (!bProgressReady || !ProgressSave)
	{
		SetFailure(
			EEverPlanetWorldFailure::SystemNotReady,
			TEXT("World progress is not ready for saving."));
		return false;
	}
	UEverPlanetWorldProgressSaveGame* JournalEntry = Cast<UEverPlanetWorldProgressSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UEverPlanetWorldProgressSaveGame::StaticClass()));
	if (!JournalEntry)
	{
		SetFailure(
			EEverPlanetWorldFailure::SaveWriteFailed,
			TEXT("Could not create the next world-progress journal entry."));
		return false;
	}
	JournalEntry->Snapshot = ProgressSave->Snapshot;
	JournalEntry->SnapshotChecksum = ComputeSnapshotChecksum(JournalEntry->Snapshot);

	const FString& TargetSlot = ActiveJournalSlotName == JournalSaveSlotA
		? JournalSaveSlotB
		: JournalSaveSlotA;
	if (!UGameplayStatics::SaveGameToSlot(JournalEntry, TargetSlot, SaveUserIndex))
	{
		SetFailure(
			EEverPlanetWorldFailure::SaveWriteFailed,
			FString::Printf(TEXT("Failed to write world-progress journal %s."), *TargetSlot));
		return false;
	}

	TObjectPtr<UEverPlanetWorldProgressSaveGame> VerifiedEntry;
	FString VerificationError;
	if (!TryLoadJournalSlot(TargetSlot, VerifiedEntry, VerificationError)
		|| VerifiedEntry->Snapshot.Revision != JournalEntry->Snapshot.Revision
		|| VerifiedEntry->SnapshotChecksum != JournalEntry->SnapshotChecksum)
	{
		SetFailure(
			EEverPlanetWorldFailure::SaveWriteFailed,
			FString::Printf(
				TEXT("World-progress journal %s failed read-back verification: %s"),
				*TargetSlot,
				*VerificationError));
		return false;
	}

	ProgressSave = VerifiedEntry;
	ActiveJournalSlotName = TargetSlot;
	return true;
}

bool UEverPlanetWorldProgressSubsystem::TryLoadJournalSlot(
	const FString& SlotName,
	TObjectPtr<UEverPlanetWorldProgressSaveGame>& OutSave,
	FString& OutError) const
{
	OutSave = Cast<UEverPlanetWorldProgressSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, SaveUserIndex));
	if (!OutSave)
	{
		OutError = FString::Printf(TEXT("Slot %s could not be loaded as world progress."), *SlotName);
		return false;
	}
	return ValidateJournalEntry(OutSave, OutError);
}

bool UEverPlanetWorldProgressSubsystem::TryLoadLegacySlot(
	TObjectPtr<UEverPlanetWorldProgressSaveGame>& OutSave,
	FString& OutError) const
{
	OutSave = Cast<UEverPlanetWorldProgressSaveGame>(
		UGameplayStatics::LoadGameFromSlot(LegacySaveSlotName, SaveUserIndex));
	if (!OutSave)
	{
		OutError = TEXT("Legacy world-progress slot could not be loaded or had the wrong class.");
		return false;
	}
	if (OutSave->SaveVersion == 1)
	{
		return ValidateSnapshotSyntax(OutSave->Snapshot, OutError);
	}
	return ValidateJournalEntry(OutSave, OutError);
}

bool UEverPlanetWorldProgressSubsystem::ValidateJournalEntry(
	const UEverPlanetWorldProgressSaveGame* Save,
	FString& OutError) const
{
	if (!Save || Save->SaveVersion != UEverPlanetWorldProgressSaveGame::CurrentSaveVersion)
	{
		OutError = FString::Printf(
			TEXT("Unsupported journal version %d; expected %d."),
			Save ? Save->SaveVersion : INDEX_NONE,
			UEverPlanetWorldProgressSaveGame::CurrentSaveVersion);
		return false;
	}
	if (!ValidateSnapshotSyntax(Save->Snapshot, OutError))
	{
		return false;
	}
	const uint32 ExpectedChecksum = ComputeSnapshotChecksum(Save->Snapshot);
	if (Save->SnapshotChecksum != ExpectedChecksum)
	{
		OutError = FString::Printf(
			TEXT("Journal checksum mismatch: stored=%u expected=%u."),
			Save->SnapshotChecksum,
			ExpectedChecksum);
		return false;
	}
	return true;
}

uint32 UEverPlanetWorldProgressSubsystem::ComputeSnapshotChecksum(
	const FEverPlanetWorldProgressSnapshot& Snapshot) const
{
	const FString Payload = EverPlanetWorldProgress::BuildCanonicalSnapshotPayload(Snapshot);
	return FCrc::StrCrc32(*Payload);
}

bool UEverPlanetWorldProgressSubsystem::SetQuestFlag(const FName QuestFlag)
{
	if (!bProgressReady || !ProgressSave)
	{
		SetFailure(EEverPlanetWorldFailure::SystemNotReady, TEXT("World progress is not ready."));
		return false;
	}
	if (!UEverPlanetWorldCatalog::IsStableId(QuestFlag))
	{
		SetFailure(
			EEverPlanetWorldFailure::InvalidIdentifier,
			FString::Printf(TEXT("Invalid QuestFlag: %s"), *QuestFlag.ToString()));
		return false;
	}
	if (ProgressSave->Snapshot.CompletedQuestFlags.Contains(QuestFlag))
	{
		ClearFailure();
		return true;
	}

	const FEverPlanetWorldProgressSnapshot Previous = ProgressSave->Snapshot;
	ProgressSave->Snapshot.CompletedQuestFlags.Add(QuestFlag);
	++ProgressSave->Snapshot.Revision;
	if (!SaveProgress())
	{
		ProgressSave->Snapshot = Previous;
		return false;
	}
	ClearFailure();
	UE_LOG(LogEverPlanet, Log, TEXT("WORLD_QUEST_FLAG_SAVED=%s"), *QuestFlag.ToString());
	return true;
}

bool UEverPlanetWorldProgressSubsystem::UnlockTravelLink(const FName LinkId)
{
	if (!bProgressReady || !ProgressSave || !ActiveCatalog)
	{
		SetFailure(EEverPlanetWorldFailure::SystemNotReady, TEXT("World progress/catalog is not ready."));
		return false;
	}
	if (!UEverPlanetWorldCatalog::IsStableId(LinkId))
	{
		SetFailure(
			EEverPlanetWorldFailure::InvalidIdentifier,
			FString::Printf(TEXT("Invalid LinkId: %s"), *LinkId.ToString()));
		return false;
	}
	const FEverPlanetTravelLinkDefinition* Link = ActiveCatalog->FindTravelLink(LinkId);
	if (!Link)
	{
		SetFailure(
			EEverPlanetWorldFailure::UnknownTravelLink,
			FString::Printf(TEXT("Cannot unlock unknown travel LinkId: %s"), *LinkId.ToString()));
		return false;
	}
	if (Link->Scope == EEverPlanetTravelScope::InterPlanet
		&& !ProgressSave->Snapshot.HasQuestFlag(Link->RequiredQuestFlag))
	{
		SetFailure(
			EEverPlanetWorldFailure::Locked,
			FString::Printf(
				TEXT("Cannot unlock interplanet travel %s before required quest flag %s is complete."),
				*LinkId.ToString(),
				*Link->RequiredQuestFlag.ToString()));
		return false;
	}
	if (ProgressSave->Snapshot.UnlockedTravelLinkIds.Contains(LinkId))
	{
		ClearFailure();
		return true;
	}

	const FEverPlanetWorldProgressSnapshot Previous = ProgressSave->Snapshot;
	ProgressSave->Snapshot.UnlockedTravelLinkIds.Add(LinkId);
	++ProgressSave->Snapshot.Revision;
	if (!SaveProgress())
	{
		ProgressSave->Snapshot = Previous;
		return false;
	}
	ClearFailure();
	UE_LOG(LogEverPlanet, Log, TEXT("WORLD_TRAVEL_LINK_UNLOCKED=%s"), *LinkId.ToString());
	return true;
}

bool UEverPlanetWorldProgressSubsystem::ValidateLoadedRegion(
	UEverPlanetWorldCatalog* Catalog,
	const FEverPlanetRegionKey& LoadedRegion,
	EEverPlanetWorldFailure& OutFailure,
	FString& OutError) const
{
	OutFailure = EEverPlanetWorldFailure::None;
	OutError.Reset();
	if (!Catalog || !LoadedRegion.IsValid())
	{
		OutFailure = EEverPlanetWorldFailure::DescriptorMismatch;
		OutError = TEXT("Loaded region descriptor has no catalog or has an invalid region key.");
		return false;
	}
	if (!Catalog->ValidateCatalogDetailed(OutFailure, OutError))
	{
		return false;
	}
	if (ActiveCatalog && ActiveCatalog != Catalog)
	{
		OutFailure = EEverPlanetWorldFailure::DescriptorMismatch;
		OutError = FString::Printf(
			TEXT("Loaded region uses catalog %s, but active catalog is %s."),
			*GetPathNameSafe(Catalog),
			*GetPathNameSafe(ActiveCatalog));
		return false;
	}

	const FEverPlanetRegionDefinition* Definition = Catalog->FindRegion(LoadedRegion.RegionId);
	if (!Definition || Definition->PlanetId != LoadedRegion.PlanetId)
	{
		OutFailure = EEverPlanetWorldFailure::DescriptorMismatch;
		OutError = FString::Printf(
			TEXT("Loaded descriptor %s does not match a catalog region."),
			*LoadedRegion.ToString());
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		OutFailure = EEverPlanetWorldFailure::SystemNotReady;
		OutError = TEXT("GameInstance subsystem has no current UWorld.");
		return false;
	}
	const FString LoadedPackageName = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	const FString CatalogPackageName = Definition->Map.ToSoftObjectPath().GetLongPackageName();
	if (LoadedPackageName != CatalogPackageName)
	{
		OutFailure = EEverPlanetWorldFailure::DescriptorMismatch;
		OutError = FString::Printf(
			TEXT("Loaded package %s does not match catalog map %s for %s."),
			*LoadedPackageName,
			*CatalogPackageName,
			*LoadedRegion.ToString());
		return false;
	}
	return true;
}

bool UEverPlanetWorldProgressSubsystem::NotifyRegionLoaded(
	UEverPlanetWorldCatalog* Catalog,
	const FEverPlanetRegionKey& LoadedRegion)
{
	if (!bProgressReady || !ProgressSave)
	{
		SetFailure(EEverPlanetWorldFailure::SystemNotReady, TEXT("World progress is not ready."));
		return false;
	}

	EEverPlanetWorldFailure Failure = EEverPlanetWorldFailure::None;
	FString Error;
	if (!ValidateLoadedRegion(Catalog, LoadedRegion, Failure, Error))
	{
		SetFailure(Failure, Error);
		return false;
	}
	ActiveCatalog = Catalog;

	if (bTransitionPending)
	{
		if (LoadedRegion != PendingTransition.DestinationAddress.GetRegionKey())
		{
			const FName LoadedPackageName(*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()));
			if (LoadedPackageName == PendingTransition.DepartureMapPackageName)
			{
				// OpenLevel has been requested but the departure world is still finishing its frame.
				return true;
			}
			RejectPendingArrival(
				EEverPlanetWorldFailure::DescriptorMismatch,
				FString::Printf(
					TEXT("Pending destination %s unexpectedly loaded descriptor %s in package %s."),
					*PendingTransition.DestinationAddress.ToString(),
					*LoadedRegion.ToString(),
					*LoadedPackageName.ToString()));
			return false;
		}
		ClearFailure();
		return true;
	}

	const FEverPlanetWorldAddress& CommittedAddress = ProgressSave->Snapshot.CurrentAddress;
	if (!CommittedAddress.IsValid())
	{
		if (LoadedRegion != Catalog->InitialAddress.GetRegionKey())
		{
			SetFailure(
				EEverPlanetWorldFailure::DescriptorMismatch,
				FString::Printf(
					TEXT("New profile may enter only initial address %s, not region %s."),
					*Catalog->InitialAddress.ToString(),
					*LoadedRegion.ToString()));
			return false;
		}
		ClearFailure();
		return true;
	}

	const FEverPlanetRegionDefinition* CommittedRegion = Catalog->FindRegion(CommittedAddress.RegionId);
	if (!CommittedRegion
		|| CommittedRegion->PlanetId != CommittedAddress.PlanetId
		|| !CommittedRegion->SpawnAnchorIds.Contains(CommittedAddress.SpawnAnchorId))
	{
		SetFailure(
			EEverPlanetWorldFailure::SaveLoadFailed,
			FString::Printf(
				TEXT("Committed address %s is not valid in the active catalog."),
				*CommittedAddress.ToString()));
		return false;
	}
	if (CommittedAddress.GetRegionKey() == LoadedRegion)
	{
		ClearFailure();
		return true;
	}

	// The project may boot through the initial map even when a profile was saved in
	// another region. Route to the committed map before allowing any pawn to spawn;
	// never overwrite the save merely because a different map was opened directly.
	return BeginResumeTransition(Catalog, CommittedAddress);
}

bool UEverPlanetWorldProgressSubsystem::RequestTravelByLinkId(
	const FName LinkId,
	const FName SourcePortalId)
{
	if (!bProgressReady || !ProgressSave || !ActiveCatalog)
	{
		SetFailure(EEverPlanetWorldFailure::SystemNotReady, TEXT("World catalog/progress is not ready."));
		return false;
	}
	if (bTransitionPending)
	{
		SetFailure(
			EEverPlanetWorldFailure::TransitionAlreadyPending,
			FString::Printf(TEXT("Transition %s is already pending."), *PendingTransition.LinkId.ToString()));
		return false;
	}
	if (!UEverPlanetWorldCatalog::IsStableId(LinkId)
		|| !UEverPlanetWorldCatalog::IsStableId(SourcePortalId))
	{
		SetFailure(
			EEverPlanetWorldFailure::InvalidIdentifier,
			TEXT("Travel LinkId or source PortalId is invalid."));
		return false;
	}
	const FEverPlanetTravelLinkDefinition* Link = ActiveCatalog->FindTravelLink(LinkId);
	if (!Link)
	{
		SetFailure(
			EEverPlanetWorldFailure::UnknownTravelLink,
			FString::Printf(TEXT("Unknown travel LinkId: %s"), *LinkId.ToString()));
		return false;
	}
	if (Link->SourcePortalId != SourcePortalId)
	{
		SetFailure(
			EEverPlanetWorldFailure::WrongSourceRegion,
			FString::Printf(
				TEXT("Travel %s requires portal %s but was invoked by %s."),
				*LinkId.ToString(),
				*Link->SourcePortalId.ToString(),
				*SourcePortalId.ToString()));
		return false;
	}
	if (ProgressSave->Snapshot.CurrentAddress.GetRegionKey() != Link->Source)
	{
		SetFailure(
			EEverPlanetWorldFailure::WrongSourceRegion,
			FString::Printf(
				TEXT("Travel %s requires source %s but current region is %s."),
				*LinkId.ToString(),
				*Link->Source.ToString(),
				*ProgressSave->Snapshot.CurrentAddress.GetRegionKey().ToString()));
		return false;
	}
	const bool bRequiredQuestComplete = ProgressSave->Snapshot.HasQuestFlag(
		Link->RequiredQuestFlag);
	if (Link->Scope == EEverPlanetTravelScope::InterPlanet
		&& !bRequiredQuestComplete)
	{
		SetFailure(
			EEverPlanetWorldFailure::Locked,
			FString::Printf(
				TEXT("Interplanet travel %s requires completed quest flag %s; an explicit unlock cannot bypass it."),
				*LinkId.ToString(),
				*Link->RequiredQuestFlag.ToString()));
		return false;
	}
	const bool bUnlocked = Link->bInitiallyUnlocked
		|| bRequiredQuestComplete
		|| ProgressSave->Snapshot.IsTravelLinkExplicitlyUnlocked(LinkId);
	if (!bUnlocked)
	{
		SetFailure(
			EEverPlanetWorldFailure::Locked,
			FString::Printf(
				TEXT("Travel %s is locked; required quest flag is %s."),
				*LinkId.ToString(),
				*Link->RequiredQuestFlag.ToString()));
		return false;
	}

	const FEverPlanetRegionDefinition* DestinationRegion = ActiveCatalog->FindRegion(
		Link->Destination.RegionId);
	if (!DestinationRegion
		|| DestinationRegion->PlanetId != Link->Destination.PlanetId
		|| DestinationRegion->Map.IsNull())
	{
		SetFailure(
			EEverPlanetWorldFailure::DestinationMapInvalid,
			FString::Printf(TEXT("Travel %s has no valid destination map."), *LinkId.ToString()));
		return false;
	}
	FString MapAvailabilityError;
	if (!IsMapPackageAvailable(DestinationRegion->Map, MapAvailabilityError))
	{
		SetFailure(EEverPlanetWorldFailure::DestinationMapInvalid, MapAvailabilityError);
		return false;
	}

	PendingTransition = FEverPlanetPendingTransition();
	PendingTransition.LinkId = LinkId;
	PendingTransition.Kind = EEverPlanetTransitionKind::PortalTravel;
	PendingTransition.SourceAddress = ProgressSave->Snapshot.CurrentAddress;
	PendingTransition.DestinationAddress = Link->Destination;
	PendingTransition.DestinationMap = DestinationRegion->Map;
	PendingTransition.DepartureMapPackageName = FName(
		*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()));
	PendingTransition.ArrivalQuestFlag = Link->ArrivalQuestFlag;
	bTransitionPending = true;
	bPendingSpawnResolved = false;
	ResolvedSpawnAnchorId = NAME_None;
	ClearFailure();
	UE_LOG(
		LogEverPlanet,
		Log,
		TEXT("WORLD_REGION_TRAVEL_REQUESTED=link=%s source=%s destination=%s map=%s"),
		*LinkId.ToString(),
		*PendingTransition.SourceAddress.ToString(),
		*PendingTransition.DestinationAddress.ToString(),
		*PendingTransition.DestinationMap.ToSoftObjectPath().ToString());
	UGameplayStatics::OpenLevelBySoftObjectPtr(
		GetGameInstance(),
		PendingTransition.DestinationMap,
		true);
	return bTransitionPending;
}

bool UEverPlanetWorldProgressSubsystem::BeginResumeTransition(
	UEverPlanetWorldCatalog* Catalog,
	const FEverPlanetWorldAddress& SavedAddress)
{
	const FEverPlanetRegionDefinition* DestinationRegion = Catalog
		? Catalog->FindRegion(SavedAddress.RegionId)
		: nullptr;
	if (!DestinationRegion
		|| DestinationRegion->PlanetId != SavedAddress.PlanetId
		|| !DestinationRegion->SpawnAnchorIds.Contains(SavedAddress.SpawnAnchorId))
	{
		SetFailure(
			EEverPlanetWorldFailure::SaveLoadFailed,
			FString::Printf(TEXT("Cannot resume invalid saved address %s."), *SavedAddress.ToString()));
		return false;
	}

	FString MapAvailabilityError;
	if (!IsMapPackageAvailable(DestinationRegion->Map, MapAvailabilityError))
	{
		SetFailure(EEverPlanetWorldFailure::DestinationMapInvalid, MapAvailabilityError);
		return false;
	}

	PendingTransition = FEverPlanetPendingTransition();
	PendingTransition.Kind = EEverPlanetTransitionKind::StartupResume;
	PendingTransition.SourceAddress = SavedAddress;
	PendingTransition.DestinationAddress = SavedAddress;
	PendingTransition.DestinationMap = DestinationRegion->Map;
	PendingTransition.DepartureMapPackageName = FName(
		*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()));
	bTransitionPending = true;
	bPendingSpawnResolved = false;
	ResolvedSpawnAnchorId = NAME_None;
	ClearFailure();
	UE_LOG(
		LogEverPlanet,
		Log,
		TEXT("WORLD_REGION_RESUME_REQUESTED=address=%s map=%s"),
		*SavedAddress.ToString(),
		*DestinationRegion->Map.ToSoftObjectPath().ToString());
	UGameplayStatics::OpenLevelBySoftObjectPtr(
		GetGameInstance(),
		DestinationRegion->Map,
		true);
	return bTransitionPending;
}

bool UEverPlanetWorldProgressSubsystem::IsMapPackageAvailable(
	const TSoftObjectPtr<UWorld>& Map,
	FString& OutError) const
{
	const FSoftObjectPath MapPath = Map.ToSoftObjectPath();
	const FString PackageName = MapPath.GetLongPackageName();
	if (!MapPath.IsValid()
		|| PackageName.IsEmpty()
		|| !FPackageName::IsValidLongPackageName(PackageName)
		|| !FPackageName::DoesPackageExist(PackageName))
	{
		OutError = FString::Printf(
			TEXT("Destination map package is missing or invalid: %s"),
			*MapPath.ToString());
		return false;
	}
	OutError.Reset();
	return true;
}

bool UEverPlanetWorldProgressSubsystem::ResolvePendingSpawn(
	UEverPlanetWorldCatalog* Catalog,
	const FEverPlanetRegionKey& LoadedRegion,
	const FName SpawnAnchorId)
{
	if (!bTransitionPending)
	{
		SetFailure(EEverPlanetWorldFailure::SystemNotReady, TEXT("No transition is pending."));
		return false;
	}

	EEverPlanetWorldFailure Failure = EEverPlanetWorldFailure::None;
	FString Error;
	if (!ValidateLoadedRegion(Catalog, LoadedRegion, Failure, Error))
	{
		SetFailure(Failure, Error);
		return false;
	}
	ActiveCatalog = Catalog;
	if (LoadedRegion != PendingTransition.DestinationAddress.GetRegionKey())
	{
		SetFailure(
			EEverPlanetWorldFailure::DescriptorMismatch,
			TEXT("Loaded region is not the pending destination."));
		return false;
	}
	if (SpawnAnchorId != PendingTransition.DestinationAddress.SpawnAnchorId)
	{
		SetFailure(
			EEverPlanetWorldFailure::MissingSpawnAnchor,
			FString::Printf(
				TEXT("Resolved spawn %s does not match pending spawn %s."),
				*SpawnAnchorId.ToString(),
				*PendingTransition.DestinationAddress.SpawnAnchorId.ToString()));
		return false;
	}

	bPendingSpawnResolved = true;
	ResolvedSpawnAnchorId = SpawnAnchorId;
	ClearFailure();
	UE_LOG(
		LogEverPlanet,
		Log,
		TEXT("WORLD_REGION_SPAWN_RESOLVED=link=%s anchor=%s"),
		*PendingTransition.LinkId.ToString(),
		*SpawnAnchorId.ToString());
	return true;
}

bool UEverPlanetWorldProgressSubsystem::ResolveDirectSpawn(
	UEverPlanetWorldCatalog* Catalog,
	const FEverPlanetRegionKey& LoadedRegion,
	FName& OutSpawnAnchorId)
{
	OutSpawnAnchorId = NAME_None;
	if (!bProgressReady || !ProgressSave || bTransitionPending)
	{
		SetFailure(
			EEverPlanetWorldFailure::SystemNotReady,
			TEXT("Direct regional spawn is unavailable while progress is not ready or travel is pending."));
		return false;
	}

	EEverPlanetWorldFailure Failure = EEverPlanetWorldFailure::None;
	FString Error;
	if (!ValidateLoadedRegion(Catalog, LoadedRegion, Failure, Error))
	{
		SetFailure(Failure, Error);
		return false;
	}
	ActiveCatalog = Catalog;

	const FEverPlanetWorldAddress& CommittedAddress = ProgressSave->Snapshot.CurrentAddress;
	const FEverPlanetWorldAddress& ExpectedAddress = CommittedAddress.IsValid()
		? CommittedAddress
		: Catalog->InitialAddress;
	if (ExpectedAddress.GetRegionKey() != LoadedRegion)
	{
		SetFailure(
			EEverPlanetWorldFailure::DescriptorMismatch,
			FString::Printf(
				TEXT("Loaded region %s is not authorized direct address %s."),
				*LoadedRegion.ToString(),
				*ExpectedAddress.ToString()));
		return false;
	}

	const FEverPlanetRegionDefinition* Definition = Catalog->FindRegion(LoadedRegion.RegionId);
	if (!Definition || !Definition->SpawnAnchorIds.Contains(ExpectedAddress.SpawnAnchorId))
	{
		SetFailure(
			EEverPlanetWorldFailure::MissingSpawnAnchor,
			FString::Printf(
				TEXT("Authorized direct address has undeclared spawn anchor %s."),
				*ExpectedAddress.SpawnAnchorId.ToString()));
		return false;
	}

	OutSpawnAnchorId = ExpectedAddress.SpawnAnchorId;
	ClearFailure();
	UE_LOG(
		LogEverPlanet,
		Log,
		TEXT("WORLD_REGION_DIRECT_SPAWN_RESOLVED=address=%s"),
		*ExpectedAddress.ToString());
	return true;
}

bool UEverPlanetWorldProgressSubsystem::CommitPendingTravel(
	const FEverPlanetRegionKey& LoadedRegion,
	const FName SpawnAnchorId)
{
	if (!bTransitionPending || !bPendingSpawnResolved || !ProgressSave)
	{
		SetFailure(
			EEverPlanetWorldFailure::SpawnNotResolved,
			TEXT("Pending travel cannot commit before its exact spawn is resolved."));
		return false;
	}
	if (LoadedRegion != PendingTransition.DestinationAddress.GetRegionKey()
		|| SpawnAnchorId != ResolvedSpawnAnchorId
		|| SpawnAnchorId != PendingTransition.DestinationAddress.SpawnAnchorId)
	{
		SetFailure(
			EEverPlanetWorldFailure::DescriptorMismatch,
			TEXT("Commit address does not match the resolved pending destination."));
		return false;
	}

	const bool bResumeOnly = PendingTransition.Kind == EEverPlanetTransitionKind::StartupResume;
	if (bResumeOnly)
	{
		if (ProgressSave->Snapshot.CurrentAddress != PendingTransition.DestinationAddress)
		{
			SetFailure(
				EEverPlanetWorldFailure::DescriptorMismatch,
				TEXT("Resume transaction destination no longer matches committed progress."));
			return false;
		}
		const FEverPlanetWorldAddress ResumedAddress = PendingTransition.DestinationAddress;
		ClearPendingTransition();
		ClearFailure();
		UE_LOG(
			LogEverPlanet,
			Log,
			TEXT("WORLD_REGION_RESUME_COMMITTED=address=%s revision=%d"),
			*ResumedAddress.ToString(),
			ProgressSave->Snapshot.Revision);
		return true;
	}

	const FEverPlanetWorldProgressSnapshot Previous = ProgressSave->Snapshot;
	ProgressSave->Snapshot.CurrentAddress = PendingTransition.DestinationAddress;
	if (!PendingTransition.ArrivalQuestFlag.IsNone()
		&& !ProgressSave->Snapshot.CompletedQuestFlags.Contains(PendingTransition.ArrivalQuestFlag))
	{
		ProgressSave->Snapshot.CompletedQuestFlags.Add(PendingTransition.ArrivalQuestFlag);
	}
	++ProgressSave->Snapshot.Revision;
	if (!SaveProgress())
	{
		ProgressSave->Snapshot = Previous;
		return false;
	}

	const FName CompletedLinkId = PendingTransition.LinkId;
	const FEverPlanetWorldAddress CommittedAddress = PendingTransition.DestinationAddress;
	ClearPendingTransition();
	ClearFailure();
	UE_LOG(
		LogEverPlanet,
		Log,
		TEXT("WORLD_REGION_TRAVEL_COMMITTED=link=%s address=%s revision=%d"),
		*CompletedLinkId.ToString(),
		*CommittedAddress.ToString(),
		ProgressSave->Snapshot.Revision);
	return true;
}

bool UEverPlanetWorldProgressSubsystem::CommitDirectArrival(
	const FEverPlanetRegionKey& LoadedRegion,
	const FName SpawnAnchorId)
{
	if (!bProgressReady || !ProgressSave || !ActiveCatalog || bTransitionPending)
	{
		SetFailure(
			EEverPlanetWorldFailure::SystemNotReady,
			TEXT("Direct regional arrival cannot commit in the current subsystem state."));
		return false;
	}

	FEverPlanetWorldAddress Candidate;
	Candidate.PlanetId = LoadedRegion.PlanetId;
	Candidate.RegionId = LoadedRegion.RegionId;
	Candidate.SpawnAnchorId = SpawnAnchorId;
	if (ProgressSave->Snapshot.CurrentAddress.IsValid())
	{
		if (ProgressSave->Snapshot.CurrentAddress != Candidate)
		{
			SetFailure(
				EEverPlanetWorldFailure::DescriptorMismatch,
				TEXT("Direct arrival tried to replace an existing committed address."));
			return false;
		}
		ClearFailure();
		return true;
	}
	if (Candidate != ActiveCatalog->InitialAddress)
	{
		SetFailure(
			EEverPlanetWorldFailure::DescriptorMismatch,
			FString::Printf(
				TEXT("New profile address %s does not equal catalog initial address %s."),
				*Candidate.ToString(),
				*ActiveCatalog->InitialAddress.ToString()));
		return false;
	}

	const FEverPlanetWorldProgressSnapshot Previous = ProgressSave->Snapshot;
	ProgressSave->Snapshot.CurrentAddress = Candidate;
	++ProgressSave->Snapshot.Revision;
	if (!SaveProgress())
	{
		ProgressSave->Snapshot = Previous;
		return false;
	}
	ClearFailure();
	UE_LOG(
		LogEverPlanet,
		Log,
		TEXT("WORLD_REGION_INITIAL_COMMITTED=address=%s revision=%d"),
		*Candidate.ToString(),
		ProgressSave->Snapshot.Revision);
	return true;
}

void UEverPlanetWorldProgressSubsystem::RejectPendingArrival(
	const EEverPlanetWorldFailure Failure,
	const FString& Message)
{
	const EEverPlanetTransitionKind RejectedKind = PendingTransition.Kind;
	const FName RejectedLinkId = PendingTransition.LinkId;
	TSoftObjectPtr<UWorld> RecoveryMap;
	if (RejectedKind == EEverPlanetTransitionKind::PortalTravel && ActiveCatalog)
	{
		const FEverPlanetRegionDefinition* SourceRegion = ActiveCatalog->FindRegion(
			PendingTransition.SourceAddress.RegionId);
		if (SourceRegion && SourceRegion->PlanetId == PendingTransition.SourceAddress.PlanetId)
		{
			RecoveryMap = SourceRegion->Map;
		}
	}
	ClearPendingTransition();
	SetFailure(Failure, Message);
	UE_LOG(
		LogEverPlanet,
		Warning,
		TEXT("WORLD_REGION_TRANSITION_ABORTED=link=%s code=%d"),
		RejectedLinkId.IsNone() ? TEXT("resume") : *RejectedLinkId.ToString(),
		static_cast<int32>(Failure));

	if (!RecoveryMap.IsNull() && GetWorld())
	{
		const FString CurrentPackage = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
		const FString RecoveryPackage = RecoveryMap.ToSoftObjectPath().GetLongPackageName();
		FString RecoveryError;
		if (CurrentPackage != RecoveryPackage && IsMapPackageAvailable(RecoveryMap, RecoveryError))
		{
			UE_LOG(
				LogEverPlanet,
				Warning,
				TEXT("WORLD_REGION_RECOVERY_REQUESTED=map=%s"),
				*RecoveryMap.ToSoftObjectPath().ToString());
			UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), RecoveryMap, true);
		}
	}
}

void UEverPlanetWorldProgressSubsystem::RejectDirectArrival(
	const EEverPlanetWorldFailure Failure,
	const FString& Message)
{
	SetFailure(Failure, Message);
}

void UEverPlanetWorldProgressSubsystem::ClearPendingTransition()
{
	PendingTransition = FEverPlanetPendingTransition();
	bTransitionPending = false;
	bPendingSpawnResolved = false;
	ResolvedSpawnAnchorId = NAME_None;
}

void UEverPlanetWorldProgressSubsystem::HandleTravelFailure(
	UWorld* FailedWorld,
	const ETravelFailure::Type FailureType,
	const FString& ErrorString)
{
	if (FailedWorld && FailedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	if (!bTransitionPending)
	{
		return;
	}
	RejectPendingArrival(
		EEverPlanetWorldFailure::DestinationMapInvalid,
		FString::Printf(
			TEXT("OpenLevel travel failed (type=%d world=%s): %s"),
			static_cast<int32>(FailureType),
			*GetNameSafe(FailedWorld),
			*ErrorString));
}

void UEverPlanetWorldProgressSubsystem::SetFailure(
	const EEverPlanetWorldFailure Failure,
	const FString& Message)
{
	LastFailure = Failure;
	LastFailureMessage = Message;
	UE_LOG(
		LogEverPlanet,
		Error,
		TEXT("WORLD_PROGRESS_FAILURE=code=%d message=%s committed=%s pending=%d"),
		static_cast<int32>(Failure),
		*Message,
		ProgressSave ? *ProgressSave->Snapshot.CurrentAddress.ToString() : TEXT("<none>"),
		bTransitionPending ? 1 : 0);
}

void UEverPlanetWorldProgressSubsystem::ClearFailure()
{
	LastFailure = EEverPlanetWorldFailure::None;
	LastFailureMessage.Reset();
}

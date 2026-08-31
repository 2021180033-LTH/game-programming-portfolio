// Copyright Epic Games, Inc. All Rights Reserved.

#include "EverPlanetGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

#include "EverPlanet.h"
#include "World/Progression/EverPlanetWorldProgressSubsystem.h"
#include "World/Travel/EverPlanetRegionDescriptor.h"
#include "World/Travel/EverPlanetRegionPlayerStart.h"

AEverPlanetGameMode::AEverPlanetGameMode()
{
	// stub
}

bool AEverPlanetGameMode::IsRegionalSpawnFlowActive() const
{
	if (bRequireRegionalSpawnFlow)
	{
		return true;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UEverPlanetWorldProgressSubsystem* ProgressSubsystem = GameInstance
		? GameInstance->GetSubsystem<UEverPlanetWorldProgressSubsystem>()
		: nullptr;
	if (ProgressSubsystem && ProgressSubsystem->IsTransitionPending())
	{
		return true;
	}
	for (TActorIterator<AEverPlanetRegionDescriptor> It(GetWorld()); It; ++It)
	{
		return true;
	}
	return false;
}

void AEverPlanetGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!IsRegionalSpawnFlowActive())
	{
		Super::RestartPlayer(NewPlayer);
		return;
	}
	if (!NewPlayer || NewPlayer->IsPendingKillPending())
	{
		return;
	}

	AActor* StartSpot = FindPlayerStart(NewPlayer);
	if (!StartSpot || ResolvedController.Get() != NewPlayer || ResolvedStart.Get() != StartSpot)
	{
		// Do not inherit GameModeBase's WorldSettings/previous-StartSpot fallbacks.
		// A regional pawn exists only after the exact descriptor+anchor gate succeeds.
		FailedToRestartPlayer(NewPlayer);
		return;
	}
	RestartPlayerAtPlayerStart(NewPlayer, StartSpot);
}

void AEverPlanetGameMode::RestartPlayerAtTransform(
	AController* NewPlayer,
	const FTransform& SpawnTransform)
{
	if (!NewPlayer || NewPlayer->IsPendingKillPending())
	{
		return;
	}
	if (IsRegionalSpawnFlowActive())
	{
		// A raw transform has no catalog identity or stable spawn-anchor proof.
		FailedToRestartPlayer(NewPlayer);
		return;
	}
	Super::RestartPlayerAtTransform(NewPlayer, SpawnTransform);
}

AActor* AEverPlanetGameMode::FindPlayerStart_Implementation(
	AController* Player,
	const FString& IncomingName)
{
	if (IsRegionalSpawnFlowActive())
	{
		// Keep the reservation state in this native gate even when a Blueprint child
		// implements ChoosePlayerStart without calling its parent.
		return ChoosePlayerStart_Implementation(Player);
	}
	return Super::FindPlayerStart_Implementation(Player, IncomingName);
}

AActor* AEverPlanetGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	ClearResolvedArrival();
	UGameInstance* GameInstance = GetGameInstance();
	UEverPlanetWorldProgressSubsystem* ProgressSubsystem = GameInstance
		? GameInstance->GetSubsystem<UEverPlanetWorldProgressSubsystem>()
		: nullptr;
	if (!ProgressSubsystem)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	TArray<AEverPlanetRegionDescriptor*> Descriptors;
	for (TActorIterator<AEverPlanetRegionDescriptor> It(GetWorld()); It; ++It)
	{
		Descriptors.Add(*It);
	}
	if (Descriptors.IsEmpty())
	{
		if (!ProgressSubsystem->IsTransitionPending())
		{
			// M1/M2 probes and the preserved legacy graybox are intentionally non-regional.
			return Super::ChoosePlayerStart_Implementation(Player);
		}

		const FEverPlanetPendingTransition Pending = ProgressSubsystem->GetPendingTransition();
		const FString LoadedPackage = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
		const FString DestinationPackage = Pending.DestinationMap.ToSoftObjectPath().GetLongPackageName();
		if (LoadedPackage == DestinationPackage)
		{
			ProgressSubsystem->RejectPendingArrival(
				EEverPlanetWorldFailure::DescriptorMismatch,
				TEXT("Destination regional map contains no region descriptor."));
		}
		else if (FName(*LoadedPackage) != Pending.DepartureMapPackageName)
		{
			ProgressSubsystem->RejectPendingArrival(
				EEverPlanetWorldFailure::DescriptorMismatch,
				TEXT("Unexpected map loaded while a regional transition was pending."));
		}
		return nullptr;
	}
	if (Descriptors.Num() != 1)
	{
		const FString Message = FString::Printf(
			TEXT("Regional arrival requires exactly one descriptor; found %d."),
			Descriptors.Num());
		if (ProgressSubsystem->IsTransitionPending())
		{
			ProgressSubsystem->RejectPendingArrival(EEverPlanetWorldFailure::DescriptorMismatch, Message);
		}
		else
		{
			ProgressSubsystem->RejectDirectArrival(EEverPlanetWorldFailure::DescriptorMismatch, Message);
		}
		return nullptr;
	}

	AEverPlanetRegionDescriptor* Descriptor = Descriptors[0];
	const FEverPlanetRegionKey LoadedRegion = Descriptor->GetRegionKey();
	if (!ProgressSubsystem->IsTransitionPending())
	{
		if (!ProgressSubsystem->NotifyRegionLoaded(
			Descriptor->GetWorldCatalog(),
			LoadedRegion))
		{
			return nullptr;
		}
	}

	FName ExpectedSpawnAnchorId = NAME_None;
	EEverPlanetResolvedArrivalMode ArrivalMode = EEverPlanetResolvedArrivalMode::Direct;
	if (ProgressSubsystem->IsTransitionPending())
	{
		const FEverPlanetPendingTransition Pending = ProgressSubsystem->GetPendingTransition();
		if (LoadedRegion != Pending.DestinationAddress.GetRegionKey())
		{
			const FString LoadedPackage = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
			const FString DestinationPackage = Pending.DestinationMap.ToSoftObjectPath().GetLongPackageName();
			if (LoadedPackage == DestinationPackage)
			{
				ProgressSubsystem->RejectPendingArrival(
					EEverPlanetWorldFailure::DescriptorMismatch,
					TEXT("Destination map descriptor does not match the pending destination region."));
			}
			else if (FName(*LoadedPackage) != Pending.DepartureMapPackageName)
			{
				ProgressSubsystem->RejectPendingArrival(
					EEverPlanetWorldFailure::DescriptorMismatch,
					TEXT("Unexpected regional map loaded while transition was pending."));
			}
			// On startup resume, the departure map remains alive until OpenLevel switches worlds.
			return nullptr;
		}
		ExpectedSpawnAnchorId = Pending.DestinationAddress.SpawnAnchorId;
		ArrivalMode = EEverPlanetResolvedArrivalMode::Pending;
	}
	else if (!ProgressSubsystem->ResolveDirectSpawn(
		Descriptor->GetWorldCatalog(),
		LoadedRegion,
		ExpectedSpawnAnchorId))
	{
		return nullptr;
	}

	TArray<AEverPlanetRegionPlayerStart*> MatchingStarts;
	for (TActorIterator<AEverPlanetRegionPlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->GetSpawnAnchorId() == ExpectedSpawnAnchorId)
		{
			MatchingStarts.Add(*It);
		}
	}
	if (MatchingStarts.Num() != 1)
	{
		const EEverPlanetWorldFailure Failure = MatchingStarts.IsEmpty()
			? EEverPlanetWorldFailure::MissingSpawnAnchor
			: EEverPlanetWorldFailure::DuplicateSpawnAnchor;
		const FString Message = FString::Printf(
			TEXT("Regional anchor %s requires exactly one PlayerStart; found %d."),
			*ExpectedSpawnAnchorId.ToString(),
			MatchingStarts.Num());
		if (ArrivalMode == EEverPlanetResolvedArrivalMode::Pending)
		{
			ProgressSubsystem->RejectPendingArrival(Failure, Message);
		}
		else
		{
			ProgressSubsystem->RejectDirectArrival(Failure, Message);
		}
		return nullptr;
	}

	AEverPlanetRegionPlayerStart* Start = MatchingStarts[0];
	if (ArrivalMode == EEverPlanetResolvedArrivalMode::Pending
		&& !ProgressSubsystem->ResolvePendingSpawn(
		Descriptor->GetWorldCatalog(),
		LoadedRegion,
		Start->GetSpawnAnchorId()))
	{
		const EEverPlanetWorldFailure Failure = ProgressSubsystem->GetLastFailure();
		const FString Message = ProgressSubsystem->GetLastFailureMessage();
		ProgressSubsystem->RejectPendingArrival(Failure, Message);
		return nullptr;
	}

	ResolvedController = Player;
	ResolvedDescriptor = Descriptor;
	ResolvedStart = Start;
	ResolvedArrivalMode = ArrivalMode;
	return Start;
}

APawn* AEverPlanetGameMode::SpawnDefaultPawnFor_Implementation(
	AController* NewPlayer,
	AActor* StartSpot)
{
	if (Cast<AEverPlanetRegionPlayerStart>(StartSpot))
	{
		// GameModeBase intentionally keeps only yaw. Regional starts need the complete
		// local-up rotation so poles and side regions do not spawn in world-Z posture.
		return SpawnDefaultPawnAtTransform(NewPlayer, StartSpot->GetActorTransform());
	}
	return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
}

void AEverPlanetGameMode::RestartPlayerAtPlayerStart(
	AController* NewPlayer,
	AActor* StartSpot)
{
	if (!NewPlayer || NewPlayer->IsPendingKillPending())
	{
		return;
	}

	const bool bResolvedRegionalStart = ResolvedController.Get() == NewPlayer
		&& ResolvedStart.IsValid()
		&& ResolvedDescriptor.IsValid()
		&& ResolvedArrivalMode != EEverPlanetResolvedArrivalMode::None
		&& StartSpot == ResolvedStart.Get();
	if (!bResolvedRegionalStart && IsRegionalSpawnFlowActive())
	{
		// Covers direct engine/Blueprint calls which bypass our RestartPlayer override.
		FailedToRestartPlayer(NewPlayer);
		return;
	}
	APawn* PawnBeforeRestart = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	if (bResolvedRegionalStart && PawnBeforeRestart)
	{
		UGameInstance* GameInstance = GetGameInstance();
		UEverPlanetWorldProgressSubsystem* ProgressSubsystem = GameInstance
			? GameInstance->GetSubsystem<UEverPlanetWorldProgressSubsystem>()
			: nullptr;
		if (ProgressSubsystem)
		{
			const FString Message = TEXT("Exact regional restart refused an already-possessed pawn.");
			if (ResolvedArrivalMode == EEverPlanetResolvedArrivalMode::Pending)
			{
				ProgressSubsystem->RejectPendingArrival(EEverPlanetWorldFailure::SpawnNotResolved, Message);
			}
			else
			{
				ProgressSubsystem->RejectDirectArrival(EEverPlanetWorldFailure::SpawnNotResolved, Message);
			}
		}
		ClearResolvedArrival();
		return;
	}
	Super::RestartPlayerAtPlayerStart(NewPlayer, StartSpot);
	if (!bResolvedRegionalStart)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UEverPlanetWorldProgressSubsystem* ProgressSubsystem = GameInstance
		? GameInstance->GetSubsystem<UEverPlanetWorldProgressSubsystem>()
		: nullptr;
	APawn* SpawnedPawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	if (!ProgressSubsystem || !SpawnedPawn)
	{
		if (ProgressSubsystem)
		{
			const FString Message = TEXT("Regional PlayerStart resolved, but the pawn did not spawn.");
			if (ResolvedArrivalMode == EEverPlanetResolvedArrivalMode::Pending)
			{
				ProgressSubsystem->RejectPendingArrival(EEverPlanetWorldFailure::SpawnNotResolved, Message);
			}
			else
			{
				ProgressSubsystem->RejectDirectArrival(EEverPlanetWorldFailure::SpawnNotResolved, Message);
			}
		}
		ClearResolvedArrival();
		return;
	}

	const FVector StartLocation = ResolvedStart->GetActorLocation();
	const bool bNewPawn = SpawnedPawn != PawnBeforeRestart;
	const bool bLocationAccepted = FVector::DistSquared(SpawnedPawn->GetActorLocation(), StartLocation)
		<= FMath::Square(100.0f);
	const bool bUpAccepted = FVector::DotProduct(
		SpawnedPawn->GetActorUpVector(),
		ResolvedStart->GetActorUpVector()) >= 0.95f;
	const bool bForwardAccepted = FVector::DotProduct(
		SpawnedPawn->GetActorForwardVector(),
		ResolvedStart->GetActorForwardVector()) >= 0.95f;
	const bool bScaleAccepted = SpawnedPawn->GetActorScale3D().Equals(
		ResolvedStart->GetActorScale3D(),
		0.01f);
	if (!bNewPawn
		|| !bLocationAccepted
		|| !bUpAccepted
		|| !bForwardAccepted
		|| !bScaleAccepted)
	{
		const FString Message = FString::Printf(
			TEXT("Regional pawn failed exact-start verification: new=%d location=%d local-up=%d forward=%d scale=%d."),
			bNewPawn ? 1 : 0,
			bLocationAccepted ? 1 : 0,
			bUpAccepted ? 1 : 0,
			bForwardAccepted ? 1 : 0,
			bScaleAccepted ? 1 : 0);
		if (ResolvedArrivalMode == EEverPlanetResolvedArrivalMode::Pending)
		{
			ProgressSubsystem->RejectPendingArrival(EEverPlanetWorldFailure::SpawnNotResolved, Message);
		}
		else
		{
			ProgressSubsystem->RejectDirectArrival(EEverPlanetWorldFailure::SpawnNotResolved, Message);
		}
		SpawnedPawn->Destroy();
		ClearResolvedArrival();
		return;
	}

	const FEverPlanetRegionKey LoadedRegion = ResolvedDescriptor->GetRegionKey();
	const FName SpawnAnchorId = ResolvedStart->GetSpawnAnchorId();
	const bool bCommitted = ResolvedArrivalMode == EEverPlanetResolvedArrivalMode::Pending
		? ProgressSubsystem->CommitPendingTravel(LoadedRegion, SpawnAnchorId)
		: ProgressSubsystem->CommitDirectArrival(LoadedRegion, SpawnAnchorId);
	if (!bCommitted)
	{
		const EEverPlanetWorldFailure Failure = ProgressSubsystem->GetLastFailure();
		const FString Message = ProgressSubsystem->GetLastFailureMessage();
		if (ResolvedArrivalMode == EEverPlanetResolvedArrivalMode::Pending)
		{
			ProgressSubsystem->RejectPendingArrival(Failure, Message);
		}
		// A failed journal write or address mismatch must not leave a playable pawn
		// in a region that committed progress never accepted.
		SpawnedPawn->Destroy();
	}
	ClearResolvedArrival();
}

void AEverPlanetGameMode::ClearResolvedArrival()
{
	ResolvedController.Reset();
	ResolvedDescriptor.Reset();
	ResolvedStart.Reset();
	ResolvedArrivalMode = EEverPlanetResolvedArrivalMode::None;
}

/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/MainGameMode.cpp
 * Original line range(s): 1256-1556
 * Attribution evidence: current git blame at the fixed commit: 301/301 lines attributed to Lee Taehyun identities
 * Review purpose: match participant ledger and reconnect restoration lifecycle
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 1256-1556 ----
void AMainGameMode::RecordMatchParticipantGold(
    int64 Ticket,
    const FString& PlayerName,
    int32 Gold)
{
    if (Ticket <= 0)
    {
        return;
    }

    FMatchParticipantGoldRecord& Record = MatchParticipantGoldLedger.FindOrAdd(Ticket);
    Record.PlayerName = PlayerName.IsEmpty()
        ? FString::Printf(TEXT("Player_%lld"), Ticket)
        : PlayerName;
    Record.Gold = FMath::Max(0, Gold);
}

void AMainGameMode::SweepExpiredDediTicketReservations(const TCHAR* Context)
{
    const double NowSeconds = FPlatformTime::Seconds();
    TArray<int64> ExpiredTickets;
    for (const TPair<int64, double>& Pair : PendingDediTicketReservations)
    {
        if (Pair.Value > 0.0 && NowSeconds >= Pair.Value)
        {
            ExpiredTickets.Add(Pair.Key);
        }
    }

    for (int64 Ticket : ExpiredTickets)
    {
        PendingDediTicketReservations.Remove(Ticket);
        UE_LOG(LogManager, Warning,
            TEXT("[DS] DediTicket ReservationExpired Ticket=%lld Context=%s"),
            Ticket,
            Context ? Context : TEXT("<NULL>"));
    }
}

void AMainGameMode::SaveDisconnectedPlayerSnapshot(AController* Exiting, int64 Ticket)
{
    if (!Exiting || Ticket <= 0)
    {
        return;
    }

    AMainPlayerState* PlayerState = Exiting->GetPlayerState<AMainPlayerState>();
    if (!PlayerState)
    {
        return;
    }

    FDisconnectedPlayerSnapshot Snapshot;
    PlayerState->CaptureReconnectSnapshot(Snapshot.PlayerState);
    Snapshot.PlayerName = PlayerState->GetPlayerName();
    Snapshot.DisconnectTimeSeconds = FPlatformTime::Seconds();
    Snapshot.ReconnectDeadlineSeconds = Snapshot.DisconnectTimeSeconds + FMath::Max(0.0f, ReconnectGraceSeconds);
    Snapshot.DisconnectRound = CurrentRound;
    Snapshot.DisconnectPhase = CurrentServerPhase;

    if (bGameStarted)
    {
        RecordMatchParticipantGold(
            Ticket,
            PlayerState->GetPlayerName(),
            PlayerState->CurPlayerData.HoldingGold);
    }

    if (APawn* Pawn = Exiting->GetPawn())
    {
        Snapshot.PawnTransform = Pawn->GetActorTransform();
        Snapshot.bHasPawnTransform = true;
    }

    if (CardGameService)
    {
        CardGameService->DetachPlayerForReconnect(Ticket, PlayerState);
    }

    DisconnectedPlayerSnapshots.Add(Ticket, MoveTemp(Snapshot));
    StartReconnectGraceTimerIfNeeded();

    DS_LOG(TEXT("[DS] Reconnect SnapshotSaved Ticket=%lld Player=%s HasTransform=%d Phase=%s Round=%d Grace=%.1f Deadline=%.3f"),
        Ticket,
        *PlayerState->GetPlayerName(),
        DisconnectedPlayerSnapshots[Ticket].bHasPawnTransform ? 1 : 0,
        GetServerPhaseName(CurrentServerPhase),
        CurrentRound,
        ReconnectGraceSeconds,
        DisconnectedPlayerSnapshots[Ticket].ReconnectDeadlineSeconds);
}

void AMainGameMode::RestoreDisconnectedPlayerSnapshot(APlayerController* NewPlayer, int64 Ticket)
{
    FDisconnectedPlayerSnapshot* Snapshot = DisconnectedPlayerSnapshots.Find(Ticket);
    AMainPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AMainPlayerState>() : nullptr;
    if (!Snapshot || !PlayerState || Ticket <= 0)
    {
        return;
    }

    if (IsReconnectGraceExpired(Ticket))
    {
        ExpireDisconnectedPlayerSnapshot(Ticket, TEXT("RestoreGraceExpired"));
        return;
    }

    PlayerState->RestoreReconnectSnapshot(Snapshot->PlayerState);
    Snapshot->bPlayerStateRestored = true;
    Snapshot->TransformRestoreDeadlineSeconds = FPlatformTime::Seconds() +
        FMath::Max(5.0f, ReconnectTransformRestoreTimeoutSeconds);

    if (CardGameService)
    {
        CardGameService->ReattachPlayerAfterReconnect(Ticket, PlayerState);
    }

    bool bTransformRestored = !Snapshot->bHasPawnTransform;
    if (Snapshot->bHasPawnTransform)
    {
        if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(NewPlayer))
        {
            Snapshot->ReconnectedController = MainPC;
            if (MainPC->GetPawn())
            {
                ++Snapshot->TransformRestoreAttempts;
                bTransformRestored = TeleportPlayerAuthoritatively(
                    MainPC,
                    Snapshot->PawnTransform,
                    TEXT("ReconnectPostLogin"));
            }
        }
    }

    DS_LOG(TEXT("[DS] Reconnect SnapshotRestored Ticket=%lld Player=%s TransformRestored=%d Phase=%s Round=%d"),
        Ticket,
        *PlayerState->GetPlayerName(),
        bTransformRestored ? 1 : 0,
        GetServerPhaseName(CurrentServerPhase),
        CurrentRound);

    if (bTransformRestored)
    {
        DisconnectedPlayerSnapshots.Remove(Ticket);
    }
    else
    {
        StartReconnectGraceTimerIfNeeded();
    }
    StopReconnectGraceTimerIfIdle();
}

bool AMainGameMode::IsReconnectGraceExpired(int64 Ticket) const
{
    const FDisconnectedPlayerSnapshot* Snapshot = DisconnectedPlayerSnapshots.Find(Ticket);
    if (!Snapshot || Snapshot->bPlayerStateRestored)
    {
        return false;
    }

    return Snapshot->ReconnectDeadlineSeconds > 0.0 &&
        FPlatformTime::Seconds() >= Snapshot->ReconnectDeadlineSeconds;
}

void AMainGameMode::ExpireDisconnectedPlayerSnapshot(int64 Ticket, const TCHAR* Reason)
{
    FDisconnectedPlayerSnapshot RemovedSnapshot;
    const bool bHadSnapshot = DisconnectedPlayerSnapshots.RemoveAndCopyValue(Ticket, RemovedSnapshot);

    if (bGameStarted && bHadSnapshot)
    {
        RecordMatchParticipantGold(
            Ticket,
            RemovedSnapshot.PlayerName,
            RemovedSnapshot.PlayerState.CurPlayerData.HoldingGold);
    }

    if (CardGameService)
    {
        CardGameService->ExpireReconnectState(Ticket, Reason);
    }

    AllowedDediTickets.Remove(Ticket);
    ActiveDediTickets.Remove(Ticket);
    PendingDediTicketReservations.Remove(Ticket);

    DS_LOG(TEXT("[DS] Reconnect GraceExpired Ticket=%lld HadSnapshot=%d DisconnectPhase=%s DisconnectRound=%d Reason=%s"),
        Ticket,
        bHadSnapshot ? 1 : 0,
        bHadSnapshot ? GetServerPhaseName(RemovedSnapshot.DisconnectPhase) : TEXT("None"),
        bHadSnapshot ? RemovedSnapshot.DisconnectRound : 0,
        Reason ? Reason : TEXT("<NULL>"));

    StopReconnectGraceTimerIfIdle();
}

void AMainGameMode::StartReconnectGraceTimerIfNeeded()
{
    if (!GetWorld() ||
        DisconnectedPlayerSnapshots.IsEmpty() ||
        GetWorld()->GetTimerManager().IsTimerActive(ReconnectGraceTimerHandle))
    {
        return;
    }

    GetWorld()->GetTimerManager().SetTimer(
        ReconnectGraceTimerHandle,
        this,
        &AMainGameMode::TickReconnectGrace,
        1.0f,
        true);
}

void AMainGameMode::StopReconnectGraceTimerIfIdle()
{
    if (!GetWorld())
    {
        return;
    }

    if (!DisconnectedPlayerSnapshots.IsEmpty())
    {
        return;
    }

    GetWorld()->GetTimerManager().ClearTimer(ReconnectGraceTimerHandle);
}

void AMainGameMode::TickReconnectGrace()
{
    TArray<int64> ExpiredTickets;
    TArray<int64> CompletedTickets;
    const double NowSeconds = FPlatformTime::Seconds();

    for (TPair<int64, FDisconnectedPlayerSnapshot>& Pair : DisconnectedPlayerSnapshots)
    {
        FDisconnectedPlayerSnapshot& Snapshot = Pair.Value;
        if (!Snapshot.bPlayerStateRestored)
        {
            if (IsReconnectGraceExpired(Pair.Key))
            {
                ExpiredTickets.Add(Pair.Key);
            }
            continue;
        }

        if (!Snapshot.bHasPawnTransform)
        {
            CompletedTickets.Add(Pair.Key);
            continue;
        }

        AMainPlayerController* ReconnectedPC = Snapshot.ReconnectedController.Get();
        if (ReconnectedPC && ReconnectedPC->GetPawn())
        {
            ++Snapshot.TransformRestoreAttempts;
            if (TeleportPlayerAuthoritatively(
                ReconnectedPC,
                Snapshot.PawnTransform,
                TEXT("ReconnectTransformRetry")))
            {
                DS_LOG(TEXT("[DS] Reconnect TransformRetryComplete Ticket=%lld Attempts=%d Player=%s"),
                    Pair.Key,
                    Snapshot.TransformRestoreAttempts,
                    *GetNameSafe(ReconnectedPC->PlayerState));
                CompletedTickets.Add(Pair.Key);
                continue;
            }
        }

        if (Snapshot.TransformRestoreDeadlineSeconds > 0.0 &&
            NowSeconds >= Snapshot.TransformRestoreDeadlineSeconds)
        {
            UE_LOG(LogManager, Error,
                TEXT("[DS] Reconnect TransformRestoreAbandoned Ticket=%lld Attempts=%d Player=%s HasController=%d HasPawn=%d Action=KeepCurrentServerTransform"),
                Pair.Key,
                Snapshot.TransformRestoreAttempts,
                *GetNameSafe(ReconnectedPC ? ReconnectedPC->PlayerState : nullptr),
                ReconnectedPC ? 1 : 0,
                ReconnectedPC && ReconnectedPC->GetPawn() ? 1 : 0);
            if (ReconnectedPC && ReconnectedPC->GetPawn())
            {
                CorrectPlayerToCurrentServerTransform(ReconnectedPC, TEXT("ReconnectTransformFallback"));
            }
            CompletedTickets.Add(Pair.Key);
        }
    }

    for (int64 Ticket : CompletedTickets)
    {
        DisconnectedPlayerSnapshots.Remove(Ticket);
    }

    for (int64 Ticket : ExpiredTickets)
    {
        ExpireDisconnectedPlayerSnapshot(Ticket, TEXT("ReconnectGraceTimer"));
    }

    StopReconnectGraceTimerIfIdle();
}



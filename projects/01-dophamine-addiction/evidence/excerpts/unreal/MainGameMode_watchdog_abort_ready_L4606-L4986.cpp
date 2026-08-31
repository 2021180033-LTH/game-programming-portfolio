/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/MainGameMode.cpp
 * Original line range(s): 4606-4986
 * Attribution evidence: current git blame at the fixed commit: 381/381 lines attributed to Lee Taehyun identities
 * Review purpose: Dedicated Server recovery watchdog, abort flow and readiness control
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 4606-4986 ----
void AMainGameMode::BeginDediRecoveryWatchdogStage(FName Stage, const TCHAR* Context)
{
    if (Stage.IsNone() || bGameEndReached || bMatchAbortRequested)
    {
        return;
    }

    if (DediRecoveryWatchdogStage == Stage && DediRecoveryLastProgressTimeSeconds > 0.0)
    {
        return;
    }

    DediRecoveryWatchdogStage = Stage;
    DediRecoveryLastProgressTimeSeconds = FPlatformTime::Seconds();
    DS_LOG(TEXT("[DS] RecoveryWatchdog StageBegin Stage=%s Context=%s Timeout=%.1f RoomId=%d Round=%d Phase=%s"),
        *Stage.ToString(),
        Context ? Context : TEXT("<NULL>"),
        Stage == FName(TEXT("WaitingPlayers"))
            ? DediPlayerJoinNoProgressTimeoutSeconds
            : DediGateNoProgressTimeoutSeconds,
        DediRoomId,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::MarkDediRecoveryProgress(const TCHAR* Context)
{
    if (DediRecoveryWatchdogStage.IsNone() || bGameEndReached || bMatchAbortRequested)
    {
        return;
    }

    const double NowSeconds = FPlatformTime::Seconds();
    const double PreviousElapsed = DediRecoveryLastProgressTimeSeconds > 0.0
        ? FMath::Max(0.0, NowSeconds - DediRecoveryLastProgressTimeSeconds)
        : 0.0;
    DediRecoveryLastProgressTimeSeconds = NowSeconds;

    DS_LOG(TEXT("[DS] RecoveryWatchdog Progress Stage=%s Context=%s PreviousIdle=%.3f RoomId=%d Round=%d Phase=%s"),
        *DediRecoveryWatchdogStage.ToString(),
        Context ? Context : TEXT("<NULL>"),
        PreviousElapsed,
        DediRoomId,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));
}

void AMainGameMode::ClearDediRecoveryWatchdogStage(const TCHAR* Context)
{
    if (DediRecoveryWatchdogStage.IsNone())
    {
        return;
    }

    const double IdleSeconds = DediRecoveryLastProgressTimeSeconds > 0.0
        ? FMath::Max(0.0, FPlatformTime::Seconds() - DediRecoveryLastProgressTimeSeconds)
        : 0.0;
    DS_LOG(TEXT("[DS] RecoveryWatchdog StageClear Stage=%s Context=%s Idle=%.3f RoomId=%d Round=%d Phase=%s"),
        *DediRecoveryWatchdogStage.ToString(),
        Context ? Context : TEXT("<NULL>"),
        IdleSeconds,
        DediRoomId,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase));

    DediRecoveryWatchdogStage = NAME_None;
    DediRecoveryLastProgressTimeSeconds = 0.0;
}

void AMainGameMode::TickDediRecoveryWatchdog()
{
    if (bGameEndReached || bMatchAbortRequested)
    {
        return;
    }

    const double NowSeconds = FPlatformTime::Seconds();
    const int32 HumanPlayers = CountConnectedHumanPlayers();

    if (bGameStarted)
    {
        if (HumanPlayers <= 0)
        {
            if (DediEmptySinceSeconds <= 0.0)
            {
                DediEmptySinceSeconds = NowSeconds;
                UE_LOG(LogTemp, Warning, TEXT("[DS] RecoveryWatchdog empty server detected RoomId=%d Round=%d Phase=%s"),
                    DediRoomId,
                    CurrentRound,
                    GetServerPhaseName(CurrentServerPhase));
            }

            const double EmptyTimeoutSeconds = FMath::Max(
                static_cast<double>(ReconnectGraceSeconds),
                static_cast<double>(DediEmptyServerAbortTimeoutSeconds));
            const double EmptyElapsedSeconds = FMath::Max(0.0, NowSeconds - DediEmptySinceSeconds);
            if (EmptyTimeoutSeconds > 0.0 && EmptyElapsedSeconds >= EmptyTimeoutSeconds)
            {
                AbortMatchAndShutdown(FString::Printf(
                    TEXT("EmptyServerTimeout:Elapsed=%.1f:Phase=%s"),
                    EmptyElapsedSeconds,
                    GetServerPhaseName(CurrentServerPhase)));
                return;
            }
        }
        else if (DediEmptySinceSeconds > 0.0)
        {
            DS_LOG(TEXT("[DS] RecoveryWatchdog empty server recovered RoomId=%d EmptyElapsed=%.3f Players=%d"),
                DediRoomId,
                FMath::Max(0.0, NowSeconds - DediEmptySinceSeconds),
                HumanPlayers);
            DediEmptySinceSeconds = 0.0;
        }
    }
    else
    {
        DediEmptySinceSeconds = 0.0;
    }

    if (DediRecoveryWatchdogStage.IsNone() || DediRecoveryLastProgressTimeSeconds <= 0.0)
    {
        return;
    }

    const double NoProgressTimeoutSeconds =
        DediRecoveryWatchdogStage == FName(TEXT("WaitingPlayers"))
        ? static_cast<double>(DediPlayerJoinNoProgressTimeoutSeconds)
        : static_cast<double>(DediGateNoProgressTimeoutSeconds);
    const double NoProgressElapsedSeconds = FMath::Max(
        0.0,
        NowSeconds - DediRecoveryLastProgressTimeSeconds);
    if (NoProgressTimeoutSeconds > 0.0 &&
        NoProgressElapsedSeconds >= NoProgressTimeoutSeconds)
    {
        AbortMatchAndShutdown(FString::Printf(
            TEXT("NoProgressTimeout:Stage=%s:Elapsed=%.1f:Players=%d"),
            *DediRecoveryWatchdogStage.ToString(),
            NoProgressElapsedSeconds,
            HumanPlayers));
    }
}

void AMainGameMode::AbortMatchAndShutdown(const FString& Reason)
{
    if (bMatchAbortRequested || bGameEndReached)
    {
        DS_LOG(TEXT("[DS] MatchAbort ignored duplicate RoomId=%d Requested=%d GameEnd=%d Reason=%s"),
            DediRoomId,
            bMatchAbortRequested ? 1 : 0,
            bGameEndReached ? 1 : 0,
            *Reason);
        return;
    }

    bMatchAbortRequested = true;
    MatchAbortReason = Reason.IsEmpty() ? TEXT("UnspecifiedMatchAbort") : Reason;
    ClearDediRecoveryWatchdogStage(TEXT("MatchAbort"));
    bGameEndReached = true;
    bGameStarted = false;

    UE_LOG(LogTemp, Error, TEXT("[DS] MatchAbort begin RoomId=%d Port=%d Generation=%u Reason=%s Round=%d Phase=%s Players=%d"),
        DediRoomId,
        DediPort,
        DediMatchGeneration,
        *MatchAbortReason,
        CurrentRound,
        GetServerPhaseName(CurrentServerPhase),
        CountConnectedHumanPlayers());

    EndPhase();
    ClearServerPhaseTimer();
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(MatchEndShutdownTimerHandle);
        TimerManager.ClearTimer(MatchEndReturnWaitTimerHandle);
        TimerManager.ClearTimer(CardSeatMoveRetryTimerHandle);
        TimerManager.ClearTimer(BattleRoyaleCardSpawnGateTimerHandle);
        TimerManager.ClearTimer(ReconnectGraceTimerHandle);
        TimerManager.ClearTimer(DediRecoveryWatchdogTimerHandle);
    }

    if (CardGameService)
    {
        CardGameService->ClearCardDrops();
        CardGameService->ClearRoundCardsForAllPlayers();
    }
    ClearGoldDrops(TEXT("MatchAbort"));
    ClearDisconnectedPlayerRoundCards(TEXT("MatchAbort"));
    SetPlayerPawnGameplayEnabled(false, TEXT("MatchAbort"));
    CurrentServerPhase = EDediServerPhase::GameEnd;
    RemainingPhaseSeconds = 0;
    SetServerRemainingTime(0);

    NotifyIocpMatchAbort(MatchAbortReason);

    if (GetWorld())
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (AMainPlayerController* MainPC = Cast<AMainPlayerController>(It->Get()))
            {
                MainPC->ClientReturnToMainMenuWithTextReason(
                    FText::FromString(FString::Printf(TEXT("MatchAborted: %s"), *MatchAbortReason)));
            }
        }

        GetWorld()->GetTimerManager().SetTimer(
            MatchAbortShutdownTimerHandle,
            this,
            &AMainGameMode::ShutdownDedicatedServerAfterMatchAbort,
            1.0f,
            false);
    }
    else
    {
        ShutdownDedicatedServerAfterMatchAbort();
    }
}

void AMainGameMode::NotifyIocpMatchAbort(const FString& Reason)
{
    constexpr uint16 MatchAbortPacketType = 304;
    constexpr uint16 MatchAbortAckType = 305;

    bMatchAbortControlNotifyComplete = false;
    bMatchAbortControlNotifySucceeded = false;
    bMatchAbortShutdownWaitLogged = false;
    MatchAbortControlNotifyStartTimeSeconds = FPlatformTime::Seconds();

    if (DediRoomId <= 0 || DediPort <= 0 || DediMatchGeneration == 0 || DediControlToken == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] IOCP MatchAbortNotify skipped. Invalid control identity RoomId=%d Port=%d Generation=%u TokenPresent=%d Reason=%s"),
            DediRoomId,
            DediPort,
            DediMatchGeneration,
            DediControlToken != 0 ? 1 : 0,
            *Reason);
        bMatchAbortControlNotifyComplete = true;
        return;
    }

    TArray<uint8> Payload;
    ManagerAppendU32BE(Payload, static_cast<uint32>(DediRoomId));
    ManagerAppendU16BE(Payload, static_cast<uint16>(DediPort));
    ManagerAppendU32BE(Payload, DediMatchGeneration);
    ManagerAppendU64BE(Payload, DediControlToken);
    ManagerAppendUtf8Limited(Payload, Reason, 180);

    TArray<uint8> Packet;
    ManagerAppendU16BE(Packet, static_cast<uint16>(4 + Payload.Num()));
    ManagerAppendU16BE(Packet, MatchAbortPacketType);
    Packet.Append(Payload);

    const TWeakObjectPtr<AMainGameMode> WeakThis(this);
    DispatchIocpControlPacketAsync(
        FString(TEXT("MatchAbortNotify")),
        DediRoomId,
        DediPort,
        DediMatchGeneration,
        DediControlToken,
        MatchAbortAckType,
        MoveTemp(Packet),
        DediIocpHost,
        DediIocpPort,
        DediControlNotifyMaxAttempts,
        DediControlAckTimeoutMs,
        DediControlRetryDelaySeconds,
        [WeakThis](bool bSucceeded)
        {
            if (AMainGameMode* GameMode = WeakThis.Get())
            {
                GameMode->HandleIocpMatchAbortNotifyComplete(bSucceeded);
            }
        });
}

void AMainGameMode::HandleIocpMatchAbortNotifyComplete(bool bSucceeded)
{
    bMatchAbortControlNotifyComplete = true;
    bMatchAbortControlNotifySucceeded = bSucceeded;
    DS_LOG(TEXT("[DS] IOCP MatchAbortNotify completion RoomId=%d Success=%d Elapsed=%.3f Reason=%s"),
        DediRoomId,
        bSucceeded ? 1 : 0,
        FMath::Max(0.0, FPlatformTime::Seconds() - MatchAbortControlNotifyStartTimeSeconds),
        *MatchAbortReason);
}

void AMainGameMode::ShutdownDedicatedServerAfterMatchAbort()
{
    constexpr double MatchAbortControlHardDeadlineSeconds = 20.0;
    const double ControlElapsedSeconds = MatchAbortControlNotifyStartTimeSeconds > 0.0
        ? FMath::Max(0.0, FPlatformTime::Seconds() - MatchAbortControlNotifyStartTimeSeconds)
        : MatchAbortControlHardDeadlineSeconds;

    if (!bMatchAbortControlNotifyComplete &&
        ControlElapsedSeconds < MatchAbortControlHardDeadlineSeconds)
    {
        if (!bMatchAbortShutdownWaitLogged)
        {
            bMatchAbortShutdownWaitLogged = true;
            UE_LOG(LogTemp, Warning, TEXT("[DS] MatchAbort shutdown waiting for IOCP RoomId=%d Elapsed=%.3f HardDeadline=%.1f Reason=%s"),
                DediRoomId,
                ControlElapsedSeconds,
                MatchAbortControlHardDeadlineSeconds,
                *MatchAbortReason);
        }

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                MatchAbortShutdownTimerHandle,
                this,
                &AMainGameMode::ShutdownDedicatedServerAfterMatchAbort,
                0.25f,
                false);
            return;
        }
    }

    if (!bMatchAbortControlNotifyComplete)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] MatchAbort shutdown reached IOCP hard deadline RoomId=%d Elapsed=%.3f Reason=%s"),
            DediRoomId,
            ControlElapsedSeconds,
            *MatchAbortReason);
    }

    UE_LOG(LogTemp, Display, TEXT("[DS] MatchAbort shutdown proceeding RoomId=%d NotifyComplete=%d NotifySucceeded=%d Elapsed=%.3f Reason=%s"),
        DediRoomId,
        bMatchAbortControlNotifyComplete ? 1 : 0,
        bMatchAbortControlNotifySucceeded ? 1 : 0,
        ControlElapsedSeconds,
        *MatchAbortReason);
    FPlatformMisc::RequestExit(false);
}

void AMainGameMode::NotifyIocpServerReady() const
{
    constexpr uint16 ServerReadyPacketType = 301;
    constexpr uint16 ServerReadyAckType = 303;

    if (DediRoomId <= 0 || DediPort <= 0 || DediMatchGeneration == 0 || DediControlToken == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[DS] IOCP ServerReadyNotify skipped. Invalid control identity RoomId=%d Port=%d Generation=%u TokenPresent=%d"),
            DediRoomId,
            DediPort,
            DediMatchGeneration,
            DediControlToken != 0 ? 1 : 0);
        return;
    }

    TArray<uint8> Payload;
    ManagerAppendU32BE(Payload, static_cast<uint32>(DediRoomId));
    ManagerAppendU16BE(Payload, static_cast<uint16>(DediPort));
    ManagerAppendU32BE(Payload, DediMatchGeneration);
    ManagerAppendU64BE(Payload, DediControlToken);

    TArray<uint8> Packet;
    const uint16 TotalSize = static_cast<uint16>(4 + Payload.Num());

    ManagerAppendU16BE(Packet, TotalSize);
    ManagerAppendU16BE(Packet, ServerReadyPacketType);
    Packet.Append(Payload);

    DispatchIocpControlPacketAsync(
        FString(TEXT("ServerReadyNotify")),
        DediRoomId,
        DediPort,
        DediMatchGeneration,
        DediControlToken,
        ServerReadyAckType,
        MoveTemp(Packet),
        DediIocpHost,
        DediIocpPort,
        DediControlNotifyMaxAttempts,
        DediControlAckTimeoutMs,
        DediControlRetryDelaySeconds,
        TFunction<void(bool)>());
}



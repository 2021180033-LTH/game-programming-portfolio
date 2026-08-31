/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/Card/CardGameService.cpp
 * Original line range(s): 1020-1088, 1090-1117
 * Attribution evidence: current git blame at the fixed commit: all 97 selected lines attributed to Lee Taehyun identities; teammate-attributed blank line 1089 is omitted
 * Review purpose: bet-turn deadline, check/die fallback and settled-round transition
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 1020-1088 ----
void UCardGameService::StartSeotdaBetTurnTimeout()
{
    UWorld* World = GetWorld();
    if (!OwnerGM || !OwnerGM->HasAuthority() || !World ||
        OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame ||
        !bSeotdaBettingActive || bSeotdaRoundResolved ||
        !GetCurrentSeotdaTurnPlayer())
    {
        return;
    }

    World->GetTimerManager().ClearTimer(SeotdaBetTurnTimeoutTimerHandle);

    const float TimeoutSeconds = OwnerGM->GetSeotdaBetTurnTimeoutSeconds();
    if (TimeoutSeconds <= 0.0f)
    {
        DS_LOG(TEXT("[DS] Seotda BetTurnTimeout disabled Round=%d"),
            OwnerGM->GetCurrentRound());
        return;
    }

    World->GetTimerManager().SetTimer(
        SeotdaBetTurnTimeoutTimerHandle,
        this,
        &UCardGameService::HandleSeotdaBetTurnTimeout,
        TimeoutSeconds,
        false);

    DS_LOG(TEXT("[DS] Seotda BetTurnTimeout started Round=%d Player=%s Seconds=%.1f"),
        OwnerGM->GetCurrentRound(),
        *GetCurrentSeotdaTurnPlayer()->GetPlayerName(),
        TimeoutSeconds);
}

void UCardGameService::HandleSeotdaBetTurnTimeout()
{
    if (!OwnerGM || !OwnerGM->HasAuthority() || !GetWorld() ||
        OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame ||
        !bSeotdaBettingActive || bSeotdaRoundResolved)
    {
        return;
    }

    AMainPlayerState* TurnPS = GetCurrentSeotdaTurnPlayer();
    FSeotdaPlayerRoundState* State = TurnPS ? SeotdaRoundStates.Find(TurnPS) : nullptr;
    if (!TurnPS || !State || !State->bSubmitted || State->bFolded || State->bAllIn)
    {
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] Seotda BetTurnTimeout invalid turn Player=%s Action=Advance"),
            *GetNameSafe(TurnPS));
        AdvanceSeotdaBettingTurn();
        return;
    }

    AMainPlayerController* TurnPC = nullptr;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        AMainPlayerController* MPC = Cast<AMainPlayerController>(It->Get());
        if (MPC && MPC->GetPlayerState<AMainPlayerState>() == TurnPS)
        {
            TurnPC = MPC;
            break;
        }
    }

    const int32 NeedCall = FMath::Max(0, SeotdaCurrentBet - State->BetMoney);
    const EBettingAction AutoAction = NeedCall > 0
        ? EBettingAction::Die
        : EBettingAction::Check;

// ---- Verbatim source lines 1090-1117 ----
    DS_LOG(TEXT("[DS] Seotda BetTurnTimeout fired Round=%d Player=%s NeedCall=%d AutoAction=%d"),
        OwnerGM->GetCurrentRound(),
        *TurnPS->GetPlayerName(),
        NeedCall,
        static_cast<int32>(AutoAction));

    if (TurnPC && SubmitSeotdaBetAction(TurnPC, AutoAction))
    {
        return;
    }

    State->bFolded = true;
    State->bActedThisBetRound = true;

    if (GetActiveSeotdaPlayerCount() <= 1 || AreSeotdaBetsSettled())
    {
        ResolveSeotdaRoundResult(TEXT("BetTurnTimeoutFallback"));
        BroadcastSeotdaState();
        if (OwnerGM->GetCurrentServerPhase() == EDediServerPhase::CardGame)
        {
            OwnerGM->FinishCurrentServerPhase(TEXT("SeotdaBetTimeoutSettled"));
        }
        return;
    }

    AdvanceSeotdaBettingTurn();
}



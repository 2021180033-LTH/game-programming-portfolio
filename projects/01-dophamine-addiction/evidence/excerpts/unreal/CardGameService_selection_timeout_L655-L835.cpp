/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/Card/CardGameService.cpp
 * Original line range(s): 655-835
 * Attribution evidence: current git blame at the fixed commit: all 181 selected lines attributed to Lee Taehyun identities; the teammate-attributed final closing brace at line 836 is omitted
 * Review purpose: selection deadline, automatic fallback selection and phase completion
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 655-835 ----
void UCardGameService::StartSeotdaSelectionTimeout()
{
    UWorld* World = GetWorld();
    if (!OwnerGM || !OwnerGM->HasAuthority() || !World ||
        OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame ||
        bSeotdaBettingActive || bSeotdaRoundResolved)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(SeotdaSelectionTimeoutTimerHandle);

    const float TimeoutSeconds = OwnerGM->GetSeotdaSelectionTimeoutSeconds();
    if (TimeoutSeconds <= 0.0f)
    {
        DS_LOG(TEXT("[DS] Seotda SelectionTimeout disabled Round=%d"),
            OwnerGM->GetCurrentRound());
        return;
    }

    // Selection is action-driven from here. Prevent the generic CardGame phase
    // timer from advancing the phase before a slow client or this timeout finishes.
    OwnerGM->ClearServerPhaseTimer();
    OwnerGM->SetRemainingPhaseSeconds(0);
    OwnerGM->SetServerRemainingTime(0);

    World->GetTimerManager().SetTimer(
        SeotdaSelectionTimeoutTimerHandle,
        this,
        &UCardGameService::HandleSeotdaSelectionTimeout,
        TimeoutSeconds,
        false);

    DS_LOG(TEXT("[DS] Seotda SelectionTimeout started Round=%d Seconds=%.1f"),
        OwnerGM->GetCurrentRound(),
        TimeoutSeconds);
}

void UCardGameService::HandleSeotdaSelectionTimeout()
{
    UWorld* World = GetWorld();
    if (!OwnerGM || !OwnerGM->HasAuthority() || !World ||
        OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame ||
        bSeotdaBettingActive || bSeotdaRoundResolved)
    {
        return;
    }

    TArray<AMainPlayerController*> PendingPlayers;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        AMainPlayerController* MPC = Cast<AMainPlayerController>(It->Get());
        AMainPlayerState* PS = MPC ? MPC->GetPlayerState<AMainPlayerState>() : nullptr;
        const FSeotdaPlayerRoundState* State = PS ? SeotdaRoundStates.Find(PS) : nullptr;
        if (MPC && PS && (!State || !State->bSubmitted))
        {
            PendingPlayers.Add(MPC);
        }
    }

    DS_LOG(TEXT("[DS] Seotda SelectionTimeout fired Round=%d Pending=%d"),
        OwnerGM->GetCurrentRound(),
        PendingPlayers.Num());

    int32 AutoRevealedCount = 0;
    int32 AutoSubmittedCount = 0;

    // First pass: publish every missing public card before any automatic hand submission.
    for (AMainPlayerController* MPC : PendingPlayers)
    {
        AMainPlayerState* PS = MPC ? MPC->GetPlayerState<AMainPlayerState>() : nullptr;
        if (!PS || PS->OwnedCards.Num() != MaxCardsPerPlayerPerRound)
        {
            UE_LOG(LogManagerCard, Error,
                TEXT("[DS] Seotda AutoReveal failed Player=%s Reason=InvalidCardCount Count=%d Required=%d"),
                *GetNameSafe(PS),
                PS ? PS->OwnedCards.Num() : 0,
                MaxCardsPerPlayerPerRound);
            continue;
        }

        FSeotdaPlayerRoundState* State = SeotdaRoundStates.Find(PS);
        if (!State || !State->bRevealConfirmed)
        {
            FString RevealFailureReason;
            const bool bRevealAccepted = RevealSeotdaCard(
                MPC,
                true,
                false,
                false,
                RevealFailureReason);

            MPC->Client_ReceiveSeotdaRevealResult(
                bRevealAccepted,
                bRevealAccepted ? FString(TEXT("SelectionTimeoutAutoReveal")) : RevealFailureReason);

            if (!bRevealAccepted)
            {
                continue;
            }

            ++AutoRevealedCount;
            DS_LOG(TEXT("[DS] Seotda AutoReveal Player=%s Instance=%d"),
                *PS->GetPlayerName(),
                PS->OwnedCards[0].CardInstanceId);
        }
    }

    // Second pass: submit a deterministic two-card hand for every player still pending.
    for (AMainPlayerController* MPC : PendingPlayers)
    {
        AMainPlayerState* PS = MPC ? MPC->GetPlayerState<AMainPlayerState>() : nullptr;
        if (!PS || PS->OwnedCards.Num() != MaxCardsPerPlayerPerRound)
        {
            UE_LOG(LogManagerCard, Error,
                TEXT("[DS] Seotda AutoSubmit failed Player=%s Reason=InvalidCardCount Count=%d Required=%d"),
                *GetNameSafe(PS),
                PS ? PS->OwnedCards.Num() : 0,
                MaxCardsPerPlayerPerRound);
            continue;
        }

        FSeotdaPlayerRoundState* State = SeotdaRoundStates.Find(PS);
        if (!State || !State->bRevealConfirmed)
        {
            UE_LOG(LogManagerCard, Error,
                TEXT("[DS] Seotda AutoSubmit failed Player=%s Reason=RevealNotConfirmed"),
                *PS->GetPlayerName());
            continue;
        }
        if (State->bSubmitted)
        {
            continue;
        }

        FString SubmitFailureReason;
        const bool bSubmitAccepted = SubmitSeotdaSelection(
            MPC,
            true,
            true,
            false,
            SubmitFailureReason);

        MPC->Client_ReceiveSeotdaSelectionResult(
            bSubmitAccepted,
            bSubmitAccepted ? FString(TEXT("SelectionTimeoutAutoSubmit")) : SubmitFailureReason);

        if (bSubmitAccepted)
        {
            ++AutoSubmittedCount;
            DS_LOG(TEXT("[DS] Seotda AutoSubmit Player=%s HandInstances=%d,%d"),
                *PS->GetPlayerName(),
                PS->OwnedCards[0].CardInstanceId,
                PS->OwnedCards[1].CardInstanceId);
        }
    }

    if (OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame ||
        bSeotdaBettingActive ||
        bSeotdaRoundResolved)
    {
        return;
    }

    TryResolveSeotdaRoundIfReady();
    if (!bSeotdaBettingActive && !bSeotdaRoundResolved)
    {
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] Seotda SelectionTimeout fallback Round=%d Pending=%d AutoRevealed=%d AutoSubmitted=%d Action=ResolveAndAdvance"),
            OwnerGM->GetCurrentRound(),
            PendingPlayers.Num(),
            AutoRevealedCount,
            AutoSubmittedCount);

        ResolveSeotdaRoundResult(TEXT("SelectionTimeoutFallback"));
        BroadcastSeotdaState();
        if (OwnerGM->GetCurrentServerPhase() == EDediServerPhase::CardGame)
        {
            OwnerGM->FinishCurrentServerPhase(TEXT("SeotdaSelectionTimeoutFallback"));
        }
    }


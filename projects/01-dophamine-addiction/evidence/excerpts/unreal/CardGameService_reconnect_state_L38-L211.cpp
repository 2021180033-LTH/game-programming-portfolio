/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/Card/CardGameService.cpp
 * Original line range(s): 38-211
 * Attribution evidence: current git blame at the fixed commit: 174/174 lines attributed to Lee Taehyun identities
 * Review purpose: detach/reattach state for reconnect and cleanup
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 38-211 ----
void UCardGameService::DetachPlayerForReconnect(int64 Ticket, AMainPlayerState* PlayerState)
{
    if (Ticket <= 0 || !PlayerState)
    {
        return;
    }

    const bool bWasBettingActive = bSeotdaBettingActive;

    if (FSeotdaPlayerRoundState* ExistingState = SeotdaRoundStates.Find(PlayerState))
    {
        FSeotdaPlayerRoundState SavedState = *ExistingState;
        SavedState.PlayerState.Reset();
        ReconnectSeotdaStates.Add(Ticket, MoveTemp(SavedState));
        SeotdaRoundStates.Remove(PlayerState);
    }

    TArray<int32> TurnIndices;
    for (int32 Index = 0; Index < SeotdaTurnOrder.Num(); ++Index)
    {
        if (SeotdaTurnOrder[Index].Get() == PlayerState)
        {
            SeotdaTurnOrder[Index].Reset();
            TurnIndices.Add(Index);
        }
    }

    if (TurnIndices.Num() > 0)
    {
        ReconnectTurnOrderIndices.Add(Ticket, MoveTemp(TurnIndices));
    }

    int32 DetachedCardCount = 0;
    for (TPair<int32, FServerCardRecord>& Pair : ServerCardRecords)
    {
        if (Pair.Value.OwnerPlayerState.Get() == PlayerState)
        {
            Pair.Value.OwnerPlayerState.Reset();
            ++DetachedCardCount;
        }
    }

    DS_LOG(TEXT("[DS] Reconnect CardStateDetached Ticket=%lld Player=%s Cards=%d HasSeotda=%d TurnSlots=%d"),
        Ticket,
        *PlayerState->GetPlayerName(),
        DetachedCardCount,
        ReconnectSeotdaStates.Contains(Ticket) ? 1 : 0,
        ReconnectTurnOrderIndices.Contains(Ticket) ? ReconnectTurnOrderIndices[Ticket].Num() : 0);

    if (bWasBettingActive && !bSeotdaRoundResolved)
    {
        if (GetActiveSeotdaPlayerCount() <= 1 || AreSeotdaBetsSettled())
        {
            ResolveSeotdaRoundResult(TEXT("DisconnectAutoFold"));
            BroadcastSeotdaState();

            if (OwnerGM && OwnerGM->GetCurrentServerPhase() == EDediServerPhase::CardGame)
            {
                OwnerGM->FinishCurrentServerPhase(TEXT("SeotdaDisconnectSettled"));
            }
        }
        else
        {
            AdvanceSeotdaBettingTurn();
        }
    }
}

void UCardGameService::ReattachPlayerAfterReconnect(int64 Ticket, AMainPlayerState* PlayerState)
{
    if (Ticket <= 0 || !PlayerState)
    {
        return;
    }

    const bool bHasSavedTurnSlots = ReconnectTurnOrderIndices.Contains(Ticket);
    if (FSeotdaPlayerRoundState* SavedState = ReconnectSeotdaStates.Find(Ticket))
    {
        if (bSeotdaBettingActive && !bHasSavedTurnSlots)
        {
            DS_LOG(TEXT("[DS] Reconnect SeotdaStateDrop Ticket=%lld Player=%s Reason=BettingStartedWithoutTurnSlot"),
                Ticket,
                *PlayerState->GetPlayerName());
        }
        else
        {
            SavedState->PlayerState = PlayerState;
            SeotdaRoundStates.Add(PlayerState, *SavedState);
        }
        ReconnectSeotdaStates.Remove(Ticket);
    }

    if (TArray<int32>* TurnIndices = ReconnectTurnOrderIndices.Find(Ticket))
    {
        for (int32 Index : *TurnIndices)
        {
            if (SeotdaTurnOrder.IsValidIndex(Index))
            {
                SeotdaTurnOrder[Index] = PlayerState;
            }
        }
        ReconnectTurnOrderIndices.Remove(Ticket);
    }

    int32 ReattachedCardCount = 0;
    for (const FOwnedCardInfo& CardInfo : PlayerState->OwnedCards)
    {
        if (FServerCardRecord* Record = ServerCardRecords.Find(CardInfo.CardInstanceId))
        {
            Record->OwnerPlayerState = PlayerState;
            ++ReattachedCardCount;
        }
    }

    DS_LOG(TEXT("[DS] Reconnect CardStateReattached Ticket=%lld Player=%s Cards=%d"),
        Ticket,
        *PlayerState->GetPlayerName(),
        ReattachedCardCount);
}

void UCardGameService::ExpireReconnectState(int64 Ticket, const TCHAR* Reason)
{
    if (Ticket <= 0)
    {
        return;
    }

    const bool bHadSeotdaState = ReconnectSeotdaStates.Remove(Ticket) > 0;
    const bool bHadTurnState = ReconnectTurnOrderIndices.Remove(Ticket) > 0;

    DS_LOG(TEXT("[DS] Reconnect CardStateExpired Ticket=%lld HasSeotda=%d HasTurn=%d Reason=%s"),
        Ticket,
        bHadSeotdaState ? 1 : 0,
        bHadTurnState ? 1 : 0,
        Reason ? Reason : TEXT("<NULL>"));
}

void UCardGameService::HandlePlayerDisconnectedAfterLogout(int64 Ticket, const TCHAR* Reason)
{
    if (!OwnerGM || OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame || bSeotdaRoundResolved)
    {
        return;
    }

    if (bSeotdaBettingActive)
    {
        return;
    }

    DS_LOG(TEXT("[DS] Seotda DisconnectAfterLogout Ticket=%lld Reason=%s"),
        Ticket,
        Reason ? Reason : TEXT("<NULL>"));

    TryResolveSeotdaRoundIfReady();
}

void UCardGameService::ClearReconnectSeotdaStateForRound(const TCHAR* Reason)
{
    const int32 SavedStateCount = ReconnectSeotdaStates.Num();
    const int32 SavedTurnCount = ReconnectTurnOrderIndices.Num();
    if (SavedStateCount <= 0 && SavedTurnCount <= 0)
    {
        return;
    }

    ReconnectSeotdaStates.Empty();
    ReconnectTurnOrderIndices.Empty();

    DS_LOG(TEXT("[DS] Reconnect SeotdaRoundStateCleared States=%d Turns=%d Reason=%s"),
        SavedStateCount,
        SavedTurnCount,
        Reason ? Reason : TEXT("<NULL>"));
}



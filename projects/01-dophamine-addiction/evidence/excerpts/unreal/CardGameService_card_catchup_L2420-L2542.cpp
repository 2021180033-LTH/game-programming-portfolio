/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/Card/CardGameService.cpp
 * Original line range(s): 2420-2542
 * Attribution evidence: current git blame at the fixed commit: 123/123 lines attributed to Lee Taehyun identities
 * Review purpose: reconnected-player card state catch-up
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 2420-2542 ----
bool UCardGameService::EnsureCardsForReconnectedPlayer(
    AMainPlayerState* PlayerState,
    const TCHAR* Context)
{
    if (!OwnerGM || !OwnerGM->HasAuthority() || !PlayerState)
    {
        return false;
    }

    const int32 TargetCardCount = FMath::Max(0, MaxCardsPerPlayerPerRound);
    const int32 BeforeCount = PlayerState->OwnedCards.Num();
    if (BeforeCount == TargetCardCount)
    {
        DS_LOG(TEXT("[DS] Reconnect CardCatchUpAlreadyComplete Player=%s Count=%d Round=%d Context=%s"),
            *PlayerState->GetPlayerName(),
            BeforeCount,
            OwnerGM->GetCurrentRound(),
            Context ? Context : TEXT("<NULL>"));
        return true;
    }

    if (BeforeCount > TargetCardCount)
    {
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] Reconnect CardCatchUpReject Player=%s Reason=TooManyCards Count=%d Required=%d Round=%d"),
            *PlayerState->GetPlayerName(),
            BeforeCount,
            TargetCardCount,
            OwnerGM->GetCurrentRound());
        return false;
    }

    if (OwnerGM->GetCurrentServerPhase() != EDediServerPhase::CardGame ||
        bSeotdaBettingActive ||
        bSeotdaRoundResolved)
    {
        DS_LOG(TEXT("[DS] Reconnect CardCatchUpSkipped Player=%s Count=%d Required=%d Phase=%s Betting=%d Resolved=%d Round=%d"),
            *PlayerState->GetPlayerName(),
            BeforeCount,
            TargetCardCount,
            OwnerGM->GetServerPhaseName(OwnerGM->GetCurrentServerPhase()),
            bSeotdaBettingActive ? 1 : 0,
            bSeotdaRoundResolved ? 1 : 0,
            OwnerGM->GetCurrentRound());
        return false;
    }

    TArray<int32> SupplementRecordIds;
    for (const TPair<int32, FServerCardRecord>& Pair : ServerCardRecords)
    {
        const FServerCardRecord& Record = Pair.Value;
        if (Record.CreatedRound == OwnerGM->GetCurrentRound() &&
            Record.State == ECardRuntimeState::Removed &&
            Record.CardID != ECardID::None)
        {
            SupplementRecordIds.Add(Pair.Key);
        }
    }

    for (int32 Index = SupplementRecordIds.Num() - 1; Index > 0; --Index)
    {
        const int32 SwapIndex = FMath::RandRange(0, Index);
        if (Index != SwapIndex)
        {
            SupplementRecordIds.Swap(Index, SwapIndex);
        }
    }

    int32 SupplementIndex = 0;
    while (PlayerState->OwnedCards.Num() < TargetCardCount)
    {
        bool bGranted = false;
        while (SupplementIndex < SupplementRecordIds.Num())
        {
            const int32 InstanceId = SupplementRecordIds[SupplementIndex++];
            if (GrantCardRecordToPlayer(InstanceId, PlayerState, Context))
            {
                bGranted = true;
                break;
            }
        }

        if (!bGranted)
        {
            bGranted = GrantNewCardToPlayer(
                PlayerState,
                PickSupplementCardIDForPlayer(PlayerState),
                TEXT("ReconnectAutoFillFallback"));
        }

        if (!bGranted)
        {
            break;
        }
    }

    const bool bComplete = PlayerState->OwnedCards.Num() == TargetCardCount;
    if (bComplete)
    {
        DS_LOG(TEXT("[DS] Reconnect CardCatchUpComplete Player=%s Before=%d After=%d Round=%d Cards=[%s] Context=%s"),
            *PlayerState->GetPlayerName(),
            BeforeCount,
            PlayerState->OwnedCards.Num(),
            OwnerGM->GetCurrentRound(),
            *GetOwnedCardsDebugString(PlayerState),
            Context ? Context : TEXT("<NULL>"));
    }
    else
    {
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] Reconnect CardCatchUpFailed Player=%s Before=%d After=%d Required=%d Round=%d Context=%s"),
            *PlayerState->GetPlayerName(),
            BeforeCount,
            PlayerState->OwnedCards.Num(),
            TargetCardCount,
            OwnerGM->GetCurrentRound(),
            Context ? Context : TEXT("<NULL>"));
    }

    return bComplete;
}




/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/Card/CardGameService.cpp
 * Original line range(s): 948-948, 1065-1071, 1228-1231, 1243-1243, 1254-1257, 1267-1267, 1275-1276, 1313-1317, 1704-1709
 * Attribution evidence: current git blame at the fixed commit: all selected semantic lines attributed to Lee Taehyun identities; surrounding shared betting core is omitted
 * Review purpose: all-in state reset/detection, timeout guard, turn exclusion and settled-state counting; the base AllIn action is shared team code
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 948-948 ----
        State->bAllIn = false;

// ---- Verbatim source lines 1065-1071 ----
    if (!TurnPS || !State || !State->bSubmitted || State->bFolded || State->bAllIn)
    {
        UE_LOG(LogManagerCard, Error,
            TEXT("[DS] Seotda BetTurnTimeout invalid turn Player=%s Action=Advance"),
            *GetNameSafe(TurnPS));
        AdvanceSeotdaBettingTurn();
        return;

// ---- Verbatim source lines 1228-1231 ----
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(SeotdaBetTurnTimeoutTimerHandle);
    }

// ---- Verbatim source lines 1243-1243 ----
        State->bAllIn = GetSeotdaPlayerMoney(PS) <= 0;

// ---- Verbatim source lines 1254-1257 ----
                if (Pair.Key != PS &&
                    Pair.Value.bSubmitted &&
                    !Pair.Value.bFolded &&
                    !Pair.Value.bAllIn)

// ---- Verbatim source lines 1267-1267 ----
    DS_LOG(TEXT("[DS] Seotda BetOK Player=%s Action=%d Paid=%d BetMoney=%d Pot=%d CurrentBet=%d Money=%d Folded=%d AllIn=%d"),

// ---- Verbatim source lines 1275-1276 ----
        State->bFolded ? 1 : 0,
        State->bAllIn ? 1 : 0);

// ---- Verbatim source lines 1313-1317 ----
        if (CandidatePS &&
            State &&
            State->bSubmitted &&
            !State->bFolded &&
            !State->bAllIn)

// ---- Verbatim source lines 1704-1709 ----
if (State.bAllIn)
{
ActedCount++;
continue;
}



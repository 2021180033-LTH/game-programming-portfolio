/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/Card/CardGameService.cpp
 * Original line range(s): 1420-1444, 1446-1448, 1450-1462, 1465-1469
 * Attribution evidence: current git blame at the fixed commit: all 46 selected lines attributed to Lee Taehyun identities; teammate-attributed blank/braces are omitted
 * Review purpose: deterministic multi-winner ordering, integer pot split and remainder distribution
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 1420-1444 ----
    TArray<AMainPlayerState*> WinnerPlayers;
    WinnerPlayers.Reserve(WinningStates.Num());
    for (FSeotdaPlayerRoundState* State : WinningStates)
    {
        if (AMainPlayerState* WinnerPS = State ? State->PlayerState.Get() : nullptr)
        {
            WinnerPlayers.AddUnique(WinnerPS);
        }
    }

    WinnerPlayers.Sort([](const AMainPlayerState& Left, const AMainPlayerState& Right)
    {
        const int32 NameCompare = Left.GetPlayerName().Compare(
            Right.GetPlayerName(),
            ESearchCase::IgnoreCase);
        if (NameCompare != 0)
        {
            return NameCompare < 0;
        }
        return Left.GetPlayerId() < Right.GetPlayerId();
    });

    const int32 WinnerCount = WinnerPlayers.Num();
    const int32 BaseShare = WinnerCount > 0 ? SeotdaPot / WinnerCount : 0;
    const int32 Remainder = WinnerCount > 0 ? SeotdaPot % WinnerCount : 0;

// ---- Verbatim source lines 1446-1448 ----
    TArray<FString> WinnerNames;
    TArray<FString> PayoutParts;
    for (int32 WinnerIndex = 0; WinnerIndex < WinnerCount; ++WinnerIndex)

// ---- Verbatim source lines 1450-1462 ----
        AMainPlayerState* WinnerPS = WinnerPlayers[WinnerIndex];
        const int32 Award = BaseShare + (WinnerIndex < Remainder ? 1 : 0);
        if (Award > 0)
        {
            WinnerPS->AddGold(Award);
        }

        WinnerNames.Add(WinnerPS->GetPlayerName());
        PayoutParts.Add(FString::Printf(
            TEXT("%s:+%d=>%d"),
            *WinnerPS->GetPlayerName(),
            Award,
            GetSeotdaPlayerMoney(WinnerPS)));

// ---- Verbatim source lines 1465-1469 ----
    FSeotdaPlayerRoundState* BestState = WinningStates[0];
    const bool bTie = WinnerCount > 1;
    const FString WinnerLabel = bTie
        ? FString::Printf(TEXT("Tie(%s)"), *FString::Join(WinnerNames, TEXT("/")))
        : WinnerNames[0];


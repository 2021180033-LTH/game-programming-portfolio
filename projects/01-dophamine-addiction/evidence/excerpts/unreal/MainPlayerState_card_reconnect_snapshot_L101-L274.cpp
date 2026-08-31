/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/MainPlayerState.cpp
 * Original line range(s): 101-274
 * Attribution evidence: current git blame at the fixed commit: 174/174 lines attributed to Lee Taehyun identities
 * Review purpose: replicated card state and reconnect snapshot application
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 101-274 ----
TArray<FOwnedCardInfo> AMainPlayerState::GetOwnedCards() const
{
	return OwnedCards;
}

FString AMainPlayerState::GetOwnedCardsDebugString() const
{
    TArray<FString> Parts;

    for (const FOwnedCardInfo& CardInfo : OwnedCards)
    {
        Parts.Add(FString::Printf(
            TEXT("#%d:%s"),
            CardInfo.CardInstanceId,
            *CardDebug::ToString(CardInfo.CardID)
        ));
    }

    return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : TEXT("Empty");
}

bool AMainPlayerState::HasOwnedCardInstance(int32 CardInstanceId) const
{
	return OwnedCards.ContainsByPredicate([CardInstanceId](const FOwnedCardInfo& CardInfo)
	{
		return CardInfo.CardInstanceId == CardInstanceId;
	});
}

void AMainPlayerState::AddOwnedCard(const FOwnedCardInfo& CardInfo)
{
	if (!HasAuthority() || CardInfo.CardInstanceId <= 0 || CardInfo.CardID == ECardID::None)
	{
		return;
	}

	if (HasOwnedCardInstance(CardInfo.CardInstanceId))
	{
		return;
	}

	OwnedCards.Add(CardInfo);
	PublicCardCount = OwnedCards.Num();
	OnOwnedCardsChangedNative.Broadcast(OwnedCards);
	ForceNetUpdate();
}

bool AMainPlayerState::RemoveOwnedCardByInstanceId(int32 CardInstanceId, FOwnedCardInfo& OutRemovedCard)
{
	if (!HasAuthority())
	{
		return false;
	}

	const int32 Index = OwnedCards.IndexOfByPredicate([CardInstanceId](const FOwnedCardInfo& CardInfo)
	{
		return CardInfo.CardInstanceId == CardInstanceId;
	});

	if (Index == INDEX_NONE)
	{
		return false;
	}

	OutRemovedCard = OwnedCards[Index];
	OwnedCards.RemoveAt(Index);
	PublicCardCount = OwnedCards.Num();
	if (RevealedCard.CardInstanceId == CardInstanceId)
	{
		ClearRevealedCard();
	}
	OnOwnedCardsChangedNative.Broadcast(OwnedCards);
	ForceNetUpdate();
	return true;
}

void AMainPlayerState::ClearOwnedCards()
{
	if (!HasAuthority())
	{
		return;
	}

	OwnedCards.Empty();
	PublicCardCount = 0;
	ClearRevealedCard();
	OnOwnedCardsChangedNative.Broadcast(OwnedCards);
	ForceNetUpdate();
}

void AMainPlayerState::SetRevealedCard(const FOwnedCardInfo& CardInfo)
{
	if (!HasAuthority() ||
		CardInfo.CardInstanceId <= 0 ||
		CardInfo.CardID == ECardID::None ||
		!HasOwnedCardInstance(CardInfo.CardInstanceId))
	{
		return;
	}

	RevealedCard = CardInfo;
	OnRep_RevealedCard();
	ForceNetUpdate();
}

void AMainPlayerState::ClearRevealedCard()
{
	if (!HasAuthority())
	{
		return;
	}

	RevealedCard = FOwnedCardInfo();
	OnRep_RevealedCard();
	ForceNetUpdate();
}

void AMainPlayerState::ResetState()
{
    if (!HasAuthority())
    {
        return;
    }

    const int32 RestoredHP = FMath::RoundToInt(GetCurrentMaxHP());
    const bool bChanged = CurPlayerData.CurrentHP != RestoredHP;
    CurPlayerData.CurrentHP = RestoredHP;
    if (bChanged)
    {
        OnHPChnageNative.Broadcast(CurPlayerData.CurrentHP);
    }
    ForceNetUpdate();
}

void AMainPlayerState::CaptureReconnectSnapshot(FMainPlayerReconnectSnapshot& OutSnapshot) const
{
	OutSnapshot.WeaponData = WeaponData;
	OutSnapshot.PlayerData = PlayerData;
	OutSnapshot.CurPlayerData = CurPlayerData;
	OutSnapshot.OwnedCards = OwnedCards;
	OutSnapshot.RevealedCard = RevealedCard;
	OutSnapshot.AccumulatedUpgrades = AccumulatedUpgrades;
	OutSnapshot.CarriedAmmoList = CarriedAmmoList;
	OutSnapshot.LastRandomUpgradeClaimedRound = LastRandomUpgradeClaimedRound;
}

void AMainPlayerState::RestoreReconnectSnapshot(const FMainPlayerReconnectSnapshot& Snapshot)
{
	if (!HasAuthority())
	{
		return;
	}

	const FCurPlayerData OldCurPlayerData = CurPlayerData;

	WeaponData = Snapshot.WeaponData;
	PlayerData = Snapshot.PlayerData;
	CurPlayerData = Snapshot.CurPlayerData;
	OwnedCards = Snapshot.OwnedCards;
	PublicCardCount = OwnedCards.Num();
	RevealedCard = Snapshot.RevealedCard;
	AccumulatedUpgrades = Snapshot.AccumulatedUpgrades;
	CarriedAmmoList = Snapshot.CarriedAmmoList;
	LastRandomUpgradeClaimedRound = Snapshot.LastRandomUpgradeClaimedRound;

	OnRep_PlayerData();
	OnRep_CurPlayerData(OldCurPlayerData);
	OnOwnedCardsChangedNative.Broadcast(OwnedCards);
	OnRep_PublicCardCount();
	OnRep_RevealedCard();
	OnRep_AccumulatedUpgrades();
	ForceNetUpdate();
}



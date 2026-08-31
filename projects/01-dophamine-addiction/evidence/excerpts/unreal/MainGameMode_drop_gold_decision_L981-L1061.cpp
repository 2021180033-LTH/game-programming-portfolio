/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/MainGameMode.cpp
 * Original line range(s): 981-1061
 * Attribution evidence: current git blame at the fixed commit: 79/81 lines attributed to Lee Taehyun identities; the final closing brace and blank line are structural lines not claimed as sole authorship
 * Review purpose: server-authoritative death-drop conditions, amount and ground location, direct actor spawn, ledger debit and initialization
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 981-1061 ----
bool AMainGameMode::DropGoldFromPlayer(AMainPlayerState* TargetPS, AActor* SourceActor)
{
    if (!HasAuthority() || !GetWorld() || !TargetPS || !SourceActor || !IsBattleRoyalePhase())
    {
        return false;
    }

    const int32 AvailableGold = FMath::Max(0, TargetPS->CurPlayerData.HoldingGold);
    const int32 DropAmount = FMath::Min(AvailableGold, FMath::Max(0, DeathGoldDropAmount));
    if (DropAmount <= 0)
    {
        DS_LOG(TEXT("[DS] GoldDrop Skip Player=%s Reason=NoGold Available=%d Configured=%d"),
            *TargetPS->GetPlayerName(),
            AvailableGold,
            DeathGoldDropAmount);
        return false;
    }

    const FVector SourceLocation = SourceActor->GetActorLocation();
    FVector DropLocation = SourceLocation + FVector(0.0f, 0.0f, DeathGoldGroundOffsetZ);

    FHitResult GroundHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DeathGoldGround), false);
    QueryParams.AddIgnoredActor(SourceActor);
    if (GetWorld()->LineTraceSingleByChannel(
        GroundHit,
        SourceLocation + FVector(0.0f, 0.0f, 200.0f),
        SourceLocation - FVector(0.0f, 0.0f, FMath::Max(100.0f, DeathGoldGroundTraceDepth)),
        ECC_Visibility,
        QueryParams))
    {
        DropLocation = GroundHit.ImpactPoint + FVector(0.0f, 0.0f, DeathGoldGroundOffsetZ);
    }

    TSubclassOf<AGoldDropActor> SpawnClass = GoldDropActorClass;
    if (!SpawnClass)
    {
        SpawnClass = AGoldDropActor::StaticClass();
    }

    FActorSpawnParameters Params;
    Params.Owner = SourceActor;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AGoldDropActor* GoldDrop = GetWorld()->SpawnActor<AGoldDropActor>(
        SpawnClass,
        DropLocation,
        FRotator::ZeroRotator,
        Params);
    if (!GoldDrop)
    {
        UE_LOG(LogManager, Error,
            TEXT("[DS] GoldDrop SpawnFail Player=%s Amount=%d Requested=%s"),
            *TargetPS->GetPlayerName(),
            DropAmount,
            *DropLocation.ToCompactString());
        return false;
    }

    TargetPS->AddGold(-DropAmount);
    ActiveGoldDrops.RemoveAll([](const TObjectPtr<AGoldDropActor>& ExistingDrop)
    {
        return !IsValid(ExistingDrop);
    });
    ActiveGoldDrops.Add(GoldDrop);
    GoldDrop->InitGoldDrop(
        DropAmount,
        TargetPS,
        DeathGoldSourcePickupLockSeconds);

    UE_LOG(LogManager, Display,
        TEXT("[DS] GoldDrop Spawned Player=%s Amount=%d Gold=%d->%d Requested=%s Actual=%s"),
        *TargetPS->GetPlayerName(),
        DropAmount,
        AvailableGold,
        TargetPS->CurPlayerData.HoldingGold,
        *DropLocation.ToCompactString(),
        *GoldDrop->GetActorLocation().ToCompactString());
    return true;
}



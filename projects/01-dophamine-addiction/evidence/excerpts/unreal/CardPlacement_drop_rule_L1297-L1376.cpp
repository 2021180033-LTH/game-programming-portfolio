/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/Card/CardPlacementService.cpp
 * Original line range(s): 1297-1376
 * Attribution evidence: 77/80 logic-bearing/current lines attributed to Lee Taehyun identities; three integration/structural lines are retained and not claimed as sole authorship
 * Review purpose: death-card ground-trace fallback and distance/no-drop/overlap/overhead-clearance validation
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 1297-1376 ----
        const FVector TraceStart(QueryPoint.X, QueryPoint.Y, DeathLocation.Z + TraceHalfHeight);
        const FVector TraceEnd(QueryPoint.X, QueryPoint.Y, DeathLocation.Z - TraceHalfHeight);

        ++TotalCandidates;

        TArray<FHitResult> TraceHits;
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CardDeathGroundFallback), false);
        QueryParams.bTraceComplex = false;
        World->LineTraceMultiByChannel(TraceHits, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

        const FHitResult* GroundHit = TraceHits.FindByPredicate([this](const FHitResult& Hit)
        {
            const UPrimitiveComponent* HitComponent = Hit.GetComponent();
            return HitComponent &&
                HitComponent->GetCollisionObjectType() != ECC_Pawn &&
                IsCardIslandSurfaceWalkable(Hit);
        });

        if (!GroundHit)
        {
            if (TraceHits.IsEmpty())
            {
                ++GroundTraceFail;
            }
            else
            {
                ++GroundWalkableFail;
            }
            continue;
        }

        const FVector Candidate = GroundHit->ImpactPoint + FVector(0.0f, 0.0f, GroundOffsetZ);
        if (FVector::Dist2D(DeathLocation, Candidate) > MaxRadius)
        {
            ++DeathDistanceFail;
            continue;
        }

        if (IsInsideNoDropZone(Candidate))
        {
            ++GroundNoDropFail;
            continue;
        }

        if (!IsFarEnoughFromDeathCards(Candidate))
        {
            ++GroundDistFail;
            continue;
        }

        if (!IsCardDropLocationClear(Candidate))
        {
            ++GroundOverlapFail;
            continue;
        }

        if (!HasOverheadClearance(Candidate))
        {
            ++GroundOverheadFail;
            continue;
        }

        OutLocation = Candidate;
        const float SurfaceSlopeDegrees = FMath::RadiansToDegrees(
            FMath::Acos(FMath::Clamp(GroundHit->ImpactNormal.Z, -1.0f, 1.0f)));
        UE_LOG(LogManagerCard, Warning,
            TEXT("[DS] Card DeathDropGroundFallback CardIndex=%d Attempt=%d ExistingCards=%d Death=%s Query=%s HitActor=%s HitComponent=%s Impact=%s Normal=%s Slope=%.1f Spawn=%s NavAvailable=%d"),
            CardIndex,
            Attempt,
            ExistingDropLocations.Num(),
            *DeathLocation.ToCompactString(),
            *QueryPoint.ToCompactString(),
            *GetNameSafe(GroundHit->GetActor()),
            *GetNameSafe(GroundHit->GetComponent()),
            *GroundHit->ImpactPoint.ToCompactString(),
            *GroundHit->ImpactNormal.ToCompactString(),
            SurfaceSlopeDegrees,
            *Candidate.ToCompactString(),
            NavSystem ? 1 : 0);
        return true;


/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/MainPlayerController.cpp
 * Original line range(s): 1013-1394
 * Attribution evidence: current git blame at the fixed commit: 382/382 lines attributed to Lee Taehyun identities
 * Review purpose: phase synchronization, streaming/card readiness ACK and server validation
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 1013-1394 ----
void AMainPlayerController::Client_SynchronizePhase_Implementation(EGamePhase ServerPhase)
{
	ApplySwitchMode(ServerPhase);
}

void AMainPlayerController::OnClientStreamLevelLoaded()
{
	if (!TryReportPendingClientLevelReady(TEXT("LoadCallback")))
	{
		SchedulePendingClientLevelReadinessRetry();
	}
}

void AMainPlayerController::OnClientStreamLevelUnloaded()
{
	UE_LOG(LogManagerCard, Verbose, TEXT("[CL] StreamLevelUnloaded Level=%s Phase=%d Local=%d"),
		*PendingClientStreamLevelToUnload.ToString(),
		static_cast<int32>(CurrentPhase),
		IsLocalPlayerController() ? 1 : 0);

	if (!TryReportPendingClientLevelReady(TEXT("UnloadCallback")))
	{
		SchedulePendingClientLevelReadinessRetry();
	}
}

bool AMainPlayerController::TryReportPendingClientLevelReady(const TCHAR* Context)
{
	if (bPendingClientLevelReadyReported || PendingClientStreamLevelToLoad.IsNone())
	{
		return bPendingClientLevelReadyReported;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const bool bPersistentTarget = IsControllerPersistentMainWorldTarget(PendingClientStreamLevelToLoad);
	ULevelStreaming* TargetStreamingLevel = bPersistentTarget
		? nullptr
		: UGameplayStatics::GetStreamingLevel(World, PendingClientStreamLevelToLoad);

	bool bTargetFound = false;
	bool bTargetLoaded = false;
	bool bTargetVisible = false;
	FString TargetObjectName;

	if (bPersistentTarget)
	{
		const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
		bTargetFound = FName(*CurrentLevelName) == PendingClientStreamLevelToLoad;
		bTargetLoaded = bTargetFound && World->PersistentLevel != nullptr && World->HasBegunPlay();
		bTargetVisible = bTargetLoaded;
		TargetObjectName = CurrentLevelName;
	}
	else
	{
		bTargetFound = TargetStreamingLevel != nullptr;
		bTargetLoaded = TargetStreamingLevel && TargetStreamingLevel->IsLevelLoaded();
		bTargetVisible = TargetStreamingLevel && TargetStreamingLevel->IsLevelVisible();
		TargetObjectName = GetNameSafe(TargetStreamingLevel);
	}

	ULevelStreaming* StreamingLevelToUnload = PendingClientStreamLevelToUnload.IsNone()
		|| IsControllerPersistentMainWorldTarget(PendingClientStreamLevelToUnload)
		? nullptr
		: UGameplayStatics::GetStreamingLevel(World, PendingClientStreamLevelToUnload);
	const bool bUnloadComplete = !StreamingLevelToUnload
		|| (!StreamingLevelToUnload->IsLevelLoaded() && !StreamingLevelToUnload->IsLevelVisible());
	const bool bReady = bTargetFound && bTargetLoaded && bTargetVisible && bUnloadComplete;

	if (!bReady)
	{
		if (PendingClientLevelReadinessRetryCount == 0
			|| PendingClientLevelReadinessRetryCount % 40 == 0)
		{
			UE_LOG(LogManagerCard, Display,
				TEXT("[CL] LevelReadyWait Target=%s Mode=%s TargetObject=%s Found=%d Loaded=%d Visible=%d Unload=%s UnloadComplete=%d Retry=%d Context=%s Phase=%d Local=%d"),
				*PendingClientStreamLevelToLoad.ToString(),
				bPersistentTarget ? TEXT("Persistent") : TEXT("Streaming"),
				*TargetObjectName,
				bTargetFound ? 1 : 0,
				bTargetLoaded ? 1 : 0,
				bTargetVisible ? 1 : 0,
				*PendingClientStreamLevelToUnload.ToString(),
				bUnloadComplete ? 1 : 0,
				PendingClientLevelReadinessRetryCount,
				Context ? Context : TEXT("<NULL>"),
				static_cast<int32>(CurrentPhase),
				IsLocalPlayerController() ? 1 : 0);
		}

		return false;
	}

	World->GetTimerManager().ClearTimer(PendingClientLevelReadinessTimerHandle);
	bPendingClientLevelReadyReported = true;

	UE_LOG(LogManagerCard, Display,
		TEXT("[CL] LevelReadyAck Target=%s Mode=%s TargetObject=%s Unload=%s UnloadComplete=1 Retry=%d Context=%s Phase=%d Local=%d"),
		*PendingClientStreamLevelToLoad.ToString(),
		bPersistentTarget ? TEXT("Persistent") : TEXT("Streaming"),
		*TargetObjectName,
		*PendingClientStreamLevelToUnload.ToString(),
		PendingClientLevelReadinessRetryCount,
		Context ? Context : TEXT("<NULL>"),
		static_cast<int32>(CurrentPhase),
		IsLocalPlayerController() ? 1 : 0);

	Server_ReportStreamLevelLoaded(PendingClientStreamLevelToLoad, CurrentPhase);
	return true;
}

void AMainPlayerController::SchedulePendingClientLevelReadinessRetry()
{
	UWorld* World = GetWorld();
	if (!World || bPendingClientLevelReadyReported
		|| World->GetTimerManager().IsTimerActive(PendingClientLevelReadinessTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		PendingClientLevelReadinessTimerHandle,
		this,
		&AMainPlayerController::RetryPendingClientLevelReadiness,
		0.25f,
		false);
}

void AMainPlayerController::RetryPendingClientLevelReadiness()
{
	if (UWorld* World = GetWorld())
	{
		// Executing one-shot timers still report active, so release the handle before rescheduling.
		World->GetTimerManager().ClearTimer(PendingClientLevelReadinessTimerHandle);
	}

	++PendingClientLevelReadinessRetryCount;
	if (!TryReportPendingClientLevelReady(TEXT("RetryTimer")))
	{
		SchedulePendingClientLevelReadinessRetry();
	}
}

bool AMainPlayerController::Server_ReportStreamLevelLoaded_Validate(FName LoadedLevel, EGamePhase ClientPhase)
{
	return !LoadedLevel.IsNone();
}

void AMainPlayerController::Server_ReportStreamLevelLoaded_Implementation(FName LoadedLevel, EGamePhase ClientPhase)
{
	if (!TryConsumeServerRpcRateLimit(
		StreamLevelAckRateLimitState,
		StreamLevelAckMinimumIntervalSeconds,
		TEXT("StreamLevelAck")))
	{
		return;
	}

	AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	if (!GM)
	{
		return;
	}

	GM->HandleClientStreamLevelLoaded(this, LoadedLevel, ClientPhase);
}

void AMainPlayerController::Client_ExpectCardBundle_Implementation(
	int32 Round,
	int32 BundleGeneration,
	const TArray<int32>& ExpectedInstanceIds)
{
	ClearPendingCardBundleExpectation();

	if (Round <= 0 || BundleGeneration <= 0 || ExpectedInstanceIds.IsEmpty())
	{
		UE_LOG(LogManagerCard, Error,
			TEXT("[CL] CardBundleExpectationRejected Round=%d Generation=%d Expected=%d"),
			Round,
			BundleGeneration,
			ExpectedInstanceIds.Num());
		return;
	}

	PendingClientCardBundleRound = Round;
	PendingClientCardBundleGeneration = BundleGeneration;
	PendingClientExpectedCardInstanceIds = ExpectedInstanceIds;
	PendingClientExpectedCardInstanceIds.Sort();
	PendingClientExpectedCardInstanceIds.SetNum(
		Algo::Unique(PendingClientExpectedCardInstanceIds));

	if (PendingClientExpectedCardInstanceIds.Num() != ExpectedInstanceIds.Num())
	{
		UE_LOG(LogManagerCard, Error,
			TEXT("[CL] CardBundleExpectationRejected Reason=DuplicateInstanceIds Round=%d Generation=%d Received=%d Unique=%d"),
			Round,
			BundleGeneration,
			ExpectedInstanceIds.Num(),
			PendingClientExpectedCardInstanceIds.Num());
		ClearPendingCardBundleExpectation();
		return;
	}

	if (!TryReportPendingCardBundleReady(TEXT("ExpectationReceived")))
	{
		SchedulePendingCardBundleReadinessRetry();
	}
}

bool AMainPlayerController::TryReportPendingCardBundleReady(const TCHAR* Context)
{
	if (bPendingClientCardBundleReadyReported
		|| PendingClientCardBundleGeneration <= 0
		|| PendingClientExpectedCardInstanceIds.IsEmpty())
	{
		return bPendingClientCardBundleReadyReported;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	TSet<int32> VisibleInstanceIds;
	for (TActorIterator<ACardDropActor> It(World); It; ++It)
	{
		const ACardDropActor* CardActor = *It;
		if (!IsValid(CardActor)
			|| CardActor->GetCardInstanceId() <= 0
			|| CardActor->GetCardID() == ECardID::None
			|| CardActor->IsPickedUp()
			|| CardActor->IsHidden())
		{
			continue;
		}

		VisibleInstanceIds.Add(CardActor->GetCardInstanceId());
	}

	TArray<int32> MissingInstanceIds;
	for (const int32 ExpectedInstanceId : PendingClientExpectedCardInstanceIds)
	{
		if (!VisibleInstanceIds.Contains(ExpectedInstanceId))
		{
			MissingInstanceIds.Add(ExpectedInstanceId);
		}
	}

	const int32 ExpectedCount = PendingClientExpectedCardInstanceIds.Num();
	const int32 VisibleExpectedCount = ExpectedCount - MissingInstanceIds.Num();
	if (!MissingInstanceIds.IsEmpty())
	{
		if (PendingClientCardBundleRetryCount == 0
			|| PendingClientCardBundleRetryCount % 20 == 0)
		{
			FString MissingIdsText;
			for (int32 Index = 0; Index < MissingInstanceIds.Num(); ++Index)
			{
				if (Index > 0)
				{
					MissingIdsText += TEXT(",");
				}
				MissingIdsText += FString::FromInt(MissingInstanceIds[Index]);
			}

			UE_LOG(LogManagerCard, Display,
				TEXT("[CL] CardBundleReadyWait Round=%d Generation=%d Visible=%d/%d Missing=[%s] Retry=%d Context=%s Local=%d"),
				PendingClientCardBundleRound,
				PendingClientCardBundleGeneration,
				VisibleExpectedCount,
				ExpectedCount,
				*MissingIdsText,
				PendingClientCardBundleRetryCount,
				Context ? Context : TEXT("<NULL>"),
				IsLocalPlayerController() ? 1 : 0);
		}

		return false;
	}

	World->GetTimerManager().ClearTimer(PendingClientCardBundleReadinessTimerHandle);
	bPendingClientCardBundleReadyReported = true;

	UE_LOG(LogManagerCard, Display,
		TEXT("[CL] CardBundleReadyAck Round=%d Generation=%d Visible=%d/%d Retry=%d Context=%s Local=%d"),
		PendingClientCardBundleRound,
		PendingClientCardBundleGeneration,
		VisibleExpectedCount,
		ExpectedCount,
		PendingClientCardBundleRetryCount,
		Context ? Context : TEXT("<NULL>"),
		IsLocalPlayerController() ? 1 : 0);

	Server_ReportCardBundleReady(
		PendingClientCardBundleRound,
		PendingClientCardBundleGeneration,
		VisibleExpectedCount);
	return true;
}

void AMainPlayerController::SchedulePendingCardBundleReadinessRetry()
{
	UWorld* World = GetWorld();
	if (!World
		|| bPendingClientCardBundleReadyReported
		|| PendingClientCardBundleGeneration <= 0
		|| World->GetTimerManager().IsTimerActive(PendingClientCardBundleReadinessTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		PendingClientCardBundleReadinessTimerHandle,
		this,
		&AMainPlayerController::RetryPendingCardBundleReadiness,
		0.25f,
		false);
}

void AMainPlayerController::RetryPendingCardBundleReadiness()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingClientCardBundleReadinessTimerHandle);
	}

	++PendingClientCardBundleRetryCount;
	if (!TryReportPendingCardBundleReady(TEXT("RetryTimer")))
	{
		SchedulePendingCardBundleReadinessRetry();
	}
}

void AMainPlayerController::ClearPendingCardBundleExpectation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingClientCardBundleReadinessTimerHandle);
	}

	PendingClientCardBundleRound = 0;
	PendingClientCardBundleGeneration = 0;
	PendingClientCardBundleRetryCount = 0;
	bPendingClientCardBundleReadyReported = false;
	PendingClientExpectedCardInstanceIds.Reset();
}

bool AMainPlayerController::Server_ReportCardBundleReady_Validate(
	int32 Round,
	int32 BundleGeneration,
	int32 VisibleCount)
{
	return Round > 0 && BundleGeneration > 0 && VisibleCount > 0 && VisibleCount <= 64;
}

void AMainPlayerController::Server_ReportCardBundleReady_Implementation(
	int32 Round,
	int32 BundleGeneration,
	int32 VisibleCount)
{
	if (!TryConsumeServerRpcRateLimit(
		CardBundleAckRateLimitState,
		CardBundleAckMinimumIntervalSeconds,
		TEXT("CardBundleAck")))
	{
		return;
	}

	AMainGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMainGameMode>() : nullptr;
	if (!GM)
	{
		return;
	}

	GM->HandleClientCardBundleReady(this, Round, BundleGeneration, VisibleCount);
}



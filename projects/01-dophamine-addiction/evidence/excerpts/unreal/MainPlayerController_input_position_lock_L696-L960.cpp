/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/MainPlayerController.cpp
 * Original line range(s): 696-960
 * Attribution evidence: current git blame at the fixed commit: 265/265 lines attributed to Lee Taehyun identities
 * Review purpose: gameplay input lock, server-authoritative position correction, NaN/current-transform checks and forced shop close
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 696-960 ----
void AMainPlayerController::SetGameplayInputLocked(bool bLocked, const TCHAR* Context)
{
	ApplyGameplayInputLock(bLocked, Context);

	if (HasAuthority())
	{
		if (!bLocked && bPendingServerPositionCorrection)
		{
			UE_LOG(LogManager, Display,
				TEXT("[DS] PositionCorrectionComplete Player=%s Sends=%d Reason=InputUnlocked Context=%s"),
				*GetNameSafe(PlayerState),
				PendingServerPositionCorrectionSendCount,
				*PendingServerPositionCorrectionContext);
			ClearPendingServerPositionCorrection();
		}

		Client_SetGameplayInputLocked(bLocked, FString(Context ? Context : TEXT("<NULL>")));
	}
}

void AMainPlayerController::StartServerAuthoritativePositionCorrection(
	const FVector& TargetLocation,
	const FRotator& TargetRotation,
	const TCHAR* Context)
{
	if (!HasAuthority())
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		UE_LOG(LogManager, Warning,
			TEXT("[DS] PositionCorrectionSkipped Reason=MissingPawn Player=%s Context=%s"),
			*GetNameSafe(PlayerState),
			Context ? Context : TEXT("<NULL>"));
		return;
	}

	if (TargetLocation.ContainsNaN() || TargetRotation.ContainsNaN())
	{
		UE_LOG(LogManager, Error,
			TEXT("[DS] PositionCorrectionRejected Reason=InvalidTransform Player=%s Location=%s Rotation=%s Context=%s"),
			*GetNameSafe(PlayerState),
			*TargetLocation.ToString(),
			*TargetRotation.ToString(),
			Context ? Context : TEXT("<NULL>"));
		return;
	}

	ClearPendingServerPositionCorrection();
	PendingServerPositionCorrectionLocation = TargetLocation;
	PendingServerPositionCorrectionRotation = TargetRotation.GetNormalized();
	PendingServerPositionCorrectionContext = Context ? Context : TEXT("<NULL>");
	PendingServerPositionCorrectionSendCount = 0;
	bPendingServerPositionCorrection = true;

	SendPendingServerPositionCorrection(TEXT("Initial"));
	if (!bPendingServerPositionCorrection)
	{
		return;
	}

	if (!bGameplayInputLocked)
	{
		UE_LOG(LogManager, Display,
			TEXT("[DS] PositionCorrectionComplete Player=%s Sends=%d Reason=InputUnlocked Context=%s"),
			*GetNameSafe(PlayerState),
			PendingServerPositionCorrectionSendCount,
			*PendingServerPositionCorrectionContext);
		ClearPendingServerPositionCorrection();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ServerPositionCorrectionRetryTimerHandle,
			this,
			&AMainPlayerController::RetryPendingServerPositionCorrection,
			ServerPositionCorrectionRetryIntervalSeconds,
			true);
	}
}

void AMainPlayerController::SendPendingServerPositionCorrection(const TCHAR* Reason)
{
	if (!bPendingServerPositionCorrection || !HasAuthority())
	{
		ClearPendingServerPositionCorrection();
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		UE_LOG(LogManager, Warning,
			TEXT("[DS] PositionCorrectionStopped Reason=MissingPawn Player=%s Sends=%d Context=%s"),
			*GetNameSafe(PlayerState),
			PendingServerPositionCorrectionSendCount,
			*PendingServerPositionCorrectionContext);
		ClearPendingServerPositionCorrection();
		return;
	}

	const FVector CurrentLocation = ControlledPawn->GetActorLocation();
	const FRotator CurrentRotation = ControlledPawn->GetActorRotation().GetNormalized();
	if (CurrentLocation.ContainsNaN() || CurrentRotation.ContainsNaN())
	{
		UE_LOG(LogManager, Error,
			TEXT("[DS] PositionCorrectionStopped Reason=InvalidPawnTransform Player=%s Location=%s Rotation=%s Context=%s"),
			*GetNameSafe(PlayerState),
			*CurrentLocation.ToString(),
			*CurrentRotation.ToString(),
			*PendingServerPositionCorrectionContext);
		ClearPendingServerPositionCorrection();
		return;
	}

	const float LocationDrift = FVector::Dist(
		CurrentLocation,
		PendingServerPositionCorrectionLocation);
	const float RotationDrift = GetControllerRotationErrorDegrees(
		CurrentRotation,
		PendingServerPositionCorrectionRotation);
	if (PendingServerPositionCorrectionSendCount > 0 &&
		(LocationDrift > ServerPositionCorrectionDriftWarningDistance ||
			RotationDrift > ServerPositionCorrectionDriftWarningDegrees))
	{
		UE_LOG(LogManager, Warning,
			TEXT("[DS] PositionCorrectionServerDrift Player=%s LocationDrift=%.2f RotationDrift=%.2f Sends=%d Context=%s"),
			*GetNameSafe(PlayerState),
			LocationDrift,
			RotationDrift,
			PendingServerPositionCorrectionSendCount,
			*PendingServerPositionCorrectionContext);
	}

	// A correction always uses the latest transform still owned by the server.
	PendingServerPositionCorrectionLocation = CurrentLocation;
	PendingServerPositionCorrectionRotation = CurrentRotation;
	ClientSetLocation(CurrentLocation, CurrentRotation);
	ControlledPawn->ForceNetUpdate();
	++PendingServerPositionCorrectionSendCount;

	DS_LOG(TEXT("[DS] PositionCorrectionSend Player=%s Send=%d/%d Reason=%s Location=%s Rotation=%s Context=%s"),
		*GetNameSafe(PlayerState),
		PendingServerPositionCorrectionSendCount,
		ServerPositionCorrectionMaxSends,
		Reason ? Reason : TEXT("<NULL>"),
		*CurrentLocation.ToString(),
		*CurrentRotation.ToString(),
		*PendingServerPositionCorrectionContext);

	if (PendingServerPositionCorrectionSendCount >= ServerPositionCorrectionMaxSends)
	{
		UE_LOG(LogManager, Display,
			TEXT("[DS] PositionCorrectionComplete Player=%s Sends=%d Reason=RetryLimit Context=%s"),
			*GetNameSafe(PlayerState),
			PendingServerPositionCorrectionSendCount,
			*PendingServerPositionCorrectionContext);
		ClearPendingServerPositionCorrection();
	}
}

void AMainPlayerController::RetryPendingServerPositionCorrection()
{
	if (!bPendingServerPositionCorrection)
	{
		ClearPendingServerPositionCorrection();
		return;
	}

	if (!bGameplayInputLocked)
	{
		UE_LOG(LogManager, Display,
			TEXT("[DS] PositionCorrectionComplete Player=%s Sends=%d Reason=InputUnlocked Context=%s"),
			*GetNameSafe(PlayerState),
			PendingServerPositionCorrectionSendCount,
			*PendingServerPositionCorrectionContext);
		ClearPendingServerPositionCorrection();
		return;
	}

	SendPendingServerPositionCorrection(TEXT("LockedRetry"));
}

void AMainPlayerController::ClearPendingServerPositionCorrection()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ServerPositionCorrectionRetryTimerHandle);
	}

	bPendingServerPositionCorrection = false;
	PendingServerPositionCorrectionSendCount = 0;
	PendingServerPositionCorrectionContext.Reset();
}

void AMainPlayerController::Client_SetGameplayInputLocked_Implementation(bool bLocked, const FString& Context)
{
	ApplyGameplayInputLock(bLocked, *Context);
}

void AMainPlayerController::ApplyGameplayInputLock(bool bLocked, const TCHAR* Context)
{
	bGameplayInputLocked = bLocked;

	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();

	if (bLocked)
	{
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
	}

	UE_LOG(LogTemp, Verbose, TEXT("[%s] GameplayInputLock Locked=%d Context=%s Local=%d Phase=%d"),
		HasAuthority() ? TEXT("SV") : TEXT("CL"),
		bLocked ? 1 : 0,
		Context ? Context : TEXT("<NULL>"),
		IsLocalPlayerController() ? 1 : 0,
		static_cast<int32>(CurrentPhase));
}

bool AMainPlayerController::ForceCloseShopMode(const TCHAR* Context)
{
	if (!HasAuthority())
	{
		return false;
	}

	const bool bWasShopOpen = ApplyForceCloseShopMode(Context);
	Client_ForceCloseShopMode(FString(Context ? Context : TEXT("<NULL>")));
	return bWasShopOpen;
}

void AMainPlayerController::Client_ForceCloseShopMode_Implementation(const FString& Context)
{
	ApplyForceCloseShopMode(*Context);
}

bool AMainPlayerController::ApplyForceCloseShopMode(const TCHAR* Context)
{
	const bool bWasShopOpen = CurrentPhase == EGamePhase::Shop;
	const int32 PendingOptionCount = CurrentUpgradeOptions.Num();
	CurrentUpgradeOptions.Empty();

	if (!bWasShopOpen)
	{
		return false;
	}

	PhaseStack.Empty();
	ApplySwitchMode(EGamePhase::TPS);

	UE_LOG(LogTemp, Display, TEXT("[%s] Shop force closed Player=%s PendingOptions=%d Context=%s"),
		HasAuthority() ? TEXT("SV") : TEXT("CL"),
		*GetNameSafe(PlayerState),
		PendingOptionCount,
		Context ? Context : TEXT("<NULL>"));
	return true;
}



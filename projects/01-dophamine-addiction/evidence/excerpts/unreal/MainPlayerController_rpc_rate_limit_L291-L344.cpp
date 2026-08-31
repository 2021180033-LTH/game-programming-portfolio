/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Source/Manager/Game/InGame/MainPlayerController.cpp
 * Original line range(s): 291-344
 * Attribution evidence: current git blame at the fixed commit: 54/54 lines attributed to Lee Taehyun identities
 * Review purpose: server RPC rate-limit gates
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 291-344 ----
bool AMainPlayerController::TryConsumeServerRpcRateLimit(
	FServerRpcRateLimitState& State,
	double MinimumIntervalSeconds,
	const TCHAR* RpcName)
{
	if (!HasAuthority())
	{
		return false;
	}

	const double NowSeconds = FPlatformTime::Seconds();
	if (!State.bHasAcceptedRequest ||
		NowSeconds - State.LastAcceptedSeconds >= MinimumIntervalSeconds)
	{
		State.LastAcceptedSeconds = NowSeconds;
		State.bHasAcceptedRequest = true;
		return true;
	}

	if (State.RejectedWindowStartSeconds <= 0.0 ||
		NowSeconds - State.RejectedWindowStartSeconds >= ServerRpcRejectedWindowSeconds)
	{
		State.RejectedWindowStartSeconds = NowSeconds;
		State.RejectedInWindow = 0;
	}

	++State.RejectedInWindow;
	if (State.RejectedInWindow >= ServerRpcRejectedWarningThreshold &&
		(State.LastWarningSeconds <= 0.0 ||
			NowSeconds - State.LastWarningSeconds >= ServerRpcWarningCooldownSeconds))
	{
		const FString PlayerLabel = PlayerState ? PlayerState->GetPlayerName() : GetName();
		UE_LOG(LogManager, Warning,
			TEXT("[DS][Security] RpcRateLimited Player=%s Controller=%s Rpc=%s Rejected=%d Window=%.1fs MinIntervalMs=%.0f"),
			*PlayerLabel,
			*GetName(),
			RpcName,
			State.RejectedInWindow,
			ServerRpcRejectedWindowSeconds,
			MinimumIntervalSeconds * 1000.0);
		State.LastWarningSeconds = NowSeconds;
	}

	return false;
}

bool AMainPlayerController::TryConsumeCardPickupRequest()
{
	return TryConsumeServerRpcRateLimit(
		CardPickupRateLimitState,
		CardPickupMinimumIntervalSeconds,
		TEXT("CardPickup"));
}



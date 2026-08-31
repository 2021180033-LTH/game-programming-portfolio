// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Planet/EverPlanetSphericalCharacter.h"
#include "M1SphericalManualTestCharacter.generated.h"

class APlanetBody;

/**
 * Test-only M1 pawn that makes manual spherical-gravity validation repeatable.
 * It observes the existing Enhanced Input path and exposes checkpoint hotkeys,
 * but deliberately does not implement the M2 local-up camera.
 */
UCLASS(Blueprintable)
class EVERPLANET_API AM1SphericalManualTestCharacter : public AEverPlanetSphericalCharacter
{
	GENERATED_BODY()

public:
	AM1SphericalManualTestCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void DoMove(float Right, float Forward) override;
	virtual void DoLook(float Yaw, float Pitch) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void OnJumped_Implementation() override;
	virtual void Landed(const FHitResult& Hit) override;

private:
	enum class EManualCheckpoint : uint8
	{
		North,
		Equator,
		South,
		PoleCross,
		FullLap
	};

	void SelectNorthCheckpoint();
	void SelectEquatorCheckpoint();
	void SelectSouthCheckpoint();
	void SelectPoleCrossCheckpoint();
	void SelectFullLapCheckpoint();
	void RestartCurrentCheckpoint();
	void ToggleProbeHud();
	void OnManualJumpInputPressed();
	void OnManualJumpInputReleased();

	void ActivateCheckpoint(EManualCheckpoint NewCheckpoint);
	bool TryGetCheckpointFrame(
		EManualCheckpoint Checkpoint,
		FVector& OutSurfaceNormal,
		FVector& OutTangentForward) const;
	bool TryGetPlanetFrame(
		FVector& OutSurfaceNormal,
		FVector& OutGravityDirection,
		double& OutCenterRadius,
		double& OutExpectedGroundRadius) const;
	void ResetCheckpointTelemetry(
		const FVector& StartNormal,
		const FVector& AuthoredTangentForward);
	void UpdateProbeTelemetry(float DeltaSeconds);
	void UpdateLapCheckpointProgress(const FVector& SurfaceNormal);
	void UpdateJumpTelemetry(
		const FVector& SurfaceNormal,
		double CenterRadius,
		double RadialSpeed);
	void DrawProbeHud(
		const FVector& SurfaceNormal,
		const FVector& GravityDirection,
		double CenterRadius,
		double ExpectedGroundRadius,
		double RadialSpeed) const;
	void RecordProbeFailure(const FString& Reason);
	const TCHAR* GetCheckpointName() const;

	UPROPERTY(EditAnywhere, Category = "M1 Manual Probe")
	bool bShowProbeHud = true;

	bool bManualProbeRuntimeEnabled = false;
	EManualCheckpoint CurrentCheckpoint = EManualCheckpoint::North;
	FVector LastMoveAxes = FVector::ZeroVector;
	FVector LastLookAxes = FVector::ZeroVector;
	FVector CheckpointStartNormal = FVector::UpVector;
	FVector CheckpointTravelAxis = FVector::UpVector;
	FVector PreviousSurfaceNormal = FVector::UpVector;
	double AccumulatedSurfaceTravelDegrees = 0.0;
	double MaximumSurfaceZ = -1.0;
	double MaximumGreatCircleDriftDegrees = 0.0;
	double JumpStartRadius = 0.0;
	double JumpPeakRadius = 0.0;
	double JumpStartedAtSeconds = 0.0;
	double JumpInputPressedAtSeconds = 0.0;
	uint64 JumpStartedFrameCounter = 0;
	uint64 JumpInputPressedFrameCounter = 0;
	uint8 LapCheckpointMask = 0;
	bool bMoveForwardSeen = false;
	bool bMoveBackwardSeen = false;
	bool bMoveRightSeen = false;
	bool bMoveLeftSeen = false;
	bool bDiagonalMoveSeen = false;
	bool bLookYawSeen = false;
	bool bLookPitchSeen = false;
	bool bJumpInputPending = false;
	bool bJumpAwaitingSpacePress = false;
	bool bSpacePressSeenForCurrentJump = false;
	bool bJumpInputReleasedSeen = false;
	bool bJumpActive = false;
	bool bJumpFallingSeen = false;
	bool bJumpOutwardSeen = false;
	bool bJumpApexSeen = false;
	bool bJumpDescendingSeen = false;
	bool bLandingEventSeen = false;
	bool bTraversalGroundIntegrityArmed = false;
	bool bPolePassLogged = false;
	bool bLapPassLogged = false;
	bool bProbeFailureLogged = false;
	TWeakObjectPtr<APlanetBody> CheckpointPlanet;
	TWeakObjectPtr<AActor> LastLandedActor;
	FString FirstProbeFailure;
};

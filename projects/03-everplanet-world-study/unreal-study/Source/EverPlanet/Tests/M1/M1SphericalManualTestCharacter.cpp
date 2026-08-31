// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/M1/M1SphericalManualTestCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "InputCoreTypes.h"
#include "Math/RotationMatrix.h"
#include "Misc/App.h"

#include "EverPlanet.h"
#include "Planet/PlanetBody.h"
#include "Planet/PlanetGravityComponent.h"
#include "Planet/PlanetGravityMath.h"

namespace EverPlanetM1ManualProbe
{
	constexpr int32 HudMessageKey = 0x4D315052;
	constexpr double SpawnClearance = 25.0;
	constexpr double GroundRadiusTolerance = 25.0;
	constexpr double AlignmentDotThreshold = 0.98;
	constexpr double GravityErrorDegreesThreshold = 1.0;
	constexpr double JumpTimeoutSeconds = 8.0;
	constexpr double MinimumJumpHeight = 25.0;
	constexpr double InputThreshold = 0.2;
	constexpr double PoleStartDegrees = 15.0;
	constexpr double GreatCircleDriftDegreesThreshold = 5.0;
	constexpr double JumpInputResponseTimeoutSeconds = 0.75;

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}
}

AM1SphericalManualTestCharacter::AM1SphericalManualTestCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bManualProbeRuntimeEnabled = FApp::CanEverRender()
		&& !FApp::IsUnattended()
		&& !GIsAutomationTesting;
}

void AM1SphericalManualTestCharacter::BeginPlay()
{
	Super::BeginPlay();

#if !UE_BUILD_SHIPPING
	bManualProbeRuntimeEnabled = bManualProbeRuntimeEnabled
		&& !GIsAutomationTesting;
	if (!bManualProbeRuntimeEnabled)
	{
		return;
	}

	FVector SurfaceNormal = FVector::UpVector;
	FVector GravityDirection = FVector::DownVector;
	double CenterRadius = 0.0;
	double ExpectedGroundRadius = 0.0;
	if (TryGetPlanetFrame(
		SurfaceNormal,
		GravityDirection,
		CenterRadius,
		ExpectedGroundRadius))
	{
		const UPlanetGravityComponent* GravityComponent = GetPlanetGravityComponent();
		CheckpointPlanet = GravityComponent ? GravityComponent->GetGravitySource() : nullptr;
		FVector InitialTangentForward = FVector::ZeroVector;
		FVector InitialTangentRight = FVector::ZeroVector;
		if (FPlanetGravityMath::TryBuildTangentBasis(
			GetActorForwardVector(),
			SurfaceNormal,
			InitialTangentForward,
			InitialTangentRight))
		{
			ResetCheckpointTelemetry(SurfaceNormal, InitialTangentForward);
		}
	}

#endif
}

void AM1SphericalManualTestCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !UE_BUILD_SHIPPING
	if (bManualProbeRuntimeEnabled && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			EverPlanetM1ManualProbe::HudMessageKey,
			0.01f,
			FColor::Transparent,
			FString());
	}
#endif

	Super::EndPlay(EndPlayReason);
}

void AM1SphericalManualTestCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

#if !UE_BUILD_SHIPPING
	if (!PlayerInputComponent || !bManualProbeRuntimeEnabled)
	{
		return;
	}

	FInputKeyBinding& NorthBinding = PlayerInputComponent->BindKey(
		EKeys::F1,
		IE_Pressed,
		this,
		&AM1SphericalManualTestCharacter::SelectNorthCheckpoint);
	NorthBinding.bConsumeInput = false;
	FInputKeyBinding& EquatorBinding = PlayerInputComponent->BindKey(
		EKeys::F2,
		IE_Pressed,
		this,
		&AM1SphericalManualTestCharacter::SelectEquatorCheckpoint);
	EquatorBinding.bConsumeInput = false;
	FInputKeyBinding& SouthBinding = PlayerInputComponent->BindKey(
		EKeys::F3,
		IE_Pressed,
		this,
		&AM1SphericalManualTestCharacter::SelectSouthCheckpoint);
	SouthBinding.bConsumeInput = false;
	FInputKeyBinding& PoleBinding = PlayerInputComponent->BindKey(
		EKeys::F4,
		IE_Pressed,
		this,
		&AM1SphericalManualTestCharacter::SelectPoleCrossCheckpoint);
	PoleBinding.bConsumeInput = false;
	FInputKeyBinding& LapBinding = PlayerInputComponent->BindKey(
		EKeys::F5,
		IE_Pressed,
		this,
		&AM1SphericalManualTestCharacter::SelectFullLapCheckpoint);
	LapBinding.bConsumeInput = false;
	FInputKeyBinding& RestartBinding = PlayerInputComponent->BindKey(
		EKeys::R,
		IE_Pressed,
		this,
		&AM1SphericalManualTestCharacter::RestartCurrentCheckpoint);
	RestartBinding.bConsumeInput = false;
	FInputKeyBinding& HudBinding = PlayerInputComponent->BindKey(
		EKeys::H,
		IE_Pressed,
		this,
		&AM1SphericalManualTestCharacter::ToggleProbeHud);
	HudBinding.bConsumeInput = false;
	FInputKeyBinding& JumpPressedBinding = PlayerInputComponent->BindKey(
		EKeys::SpaceBar,
		IE_Pressed,
		this,
		&AM1SphericalManualTestCharacter::OnManualJumpInputPressed);
	JumpPressedBinding.bConsumeInput = false;
	FInputKeyBinding& JumpReleasedBinding = PlayerInputComponent->BindKey(
		EKeys::SpaceBar,
		IE_Released,
		this,
		&AM1SphericalManualTestCharacter::OnManualJumpInputReleased);
	JumpReleasedBinding.bConsumeInput = false;
	UE_LOG(
		LogEverPlanet,
		Display,
		TEXT("M1_MANUAL_READY pawn=%s keys=F1/F2/F3/F4/F5,R,H,Space viewport_focus_required=true"),
		*GetNameSafe(this));
#endif
}

void AM1SphericalManualTestCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if !UE_BUILD_SHIPPING
	if (bManualProbeRuntimeEnabled && IsLocallyControlled())
	{
		UpdateProbeTelemetry(DeltaSeconds);
	}
#endif
}

void AM1SphericalManualTestCharacter::DoMove(const float Right, const float Forward)
{
	Super::DoMove(Right, Forward);

#if !UE_BUILD_SHIPPING
	if (!bManualProbeRuntimeEnabled)
	{
		return;
	}

	LastMoveAxes = FVector(Right, Forward, 0.0);
	const bool bHadAnyMove = bMoveForwardSeen
		|| bMoveBackwardSeen
		|| bMoveRightSeen
		|| bMoveLeftSeen;
	bMoveForwardSeen |= Forward > EverPlanetM1ManualProbe::InputThreshold;
	bMoveBackwardSeen |= Forward < -EverPlanetM1ManualProbe::InputThreshold;
	bMoveRightSeen |= Right > EverPlanetM1ManualProbe::InputThreshold;
	bMoveLeftSeen |= Right < -EverPlanetM1ManualProbe::InputThreshold;
	bDiagonalMoveSeen |= FMath::Abs(Right) > EverPlanetM1ManualProbe::InputThreshold
		&& FMath::Abs(Forward) > EverPlanetM1ManualProbe::InputThreshold;
	if (!bHadAnyMove
		&& (bMoveForwardSeen || bMoveBackwardSeen || bMoveRightSeen || bMoveLeftSeen))
	{
		UE_LOG(
			LogEverPlanet,
			Display,
			TEXT("M1_MANUAL_INPUT move right=%.3f forward=%.3f checkpoint=%s"),
			Right,
			Forward,
			GetCheckpointName());
	}
#endif
}

void AM1SphericalManualTestCharacter::DoLook(const float Yaw, const float Pitch)
{
	Super::DoLook(Yaw, Pitch);

#if !UE_BUILD_SHIPPING
	if (!bManualProbeRuntimeEnabled)
	{
		return;
	}

	LastLookAxes = FVector(Yaw, Pitch, 0.0);
	const bool bHadAnyLook = bLookYawSeen || bLookPitchSeen;
	bLookYawSeen |= FMath::Abs(Yaw) > UE_KINDA_SMALL_NUMBER;
	bLookPitchSeen |= FMath::Abs(Pitch) > UE_KINDA_SMALL_NUMBER;
	if (!bHadAnyLook && (bLookYawSeen || bLookPitchSeen))
	{
		UE_LOG(
			LogEverPlanet,
			Display,
			TEXT("M1_MANUAL_INPUT look yaw=%.3f pitch=%.3f checkpoint=%s"),
			Yaw,
			Pitch,
			GetCheckpointName());
	}
#endif
}

void AM1SphericalManualTestCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

#if !UE_BUILD_SHIPPING
	if (!bManualProbeRuntimeEnabled)
	{
		return;
	}

	FVector SurfaceNormal = FVector::ZeroVector;
	FVector GravityDirection = FVector::ZeroVector;
	double CenterRadius = 0.0;
	double ExpectedGroundRadius = 0.0;
	if (!TryGetPlanetFrame(
		SurfaceNormal,
		GravityDirection,
		CenterRadius,
		ExpectedGroundRadius))
	{
		RecordProbeFailure(TEXT("jump started without a valid planet frame"));
		return;
	}

	const double CurrentTime = GetWorld()
		? static_cast<double>(GetWorld()->GetTimeSeconds())
		: 0.0;
	const uint64 CurrentFrameCounter = GFrameCounter;
	const bool bRecentSpacePress = bJumpInputPending
		&& CurrentFrameCounter >= JumpInputPressedFrameCounter
		&& CurrentFrameCounter - JumpInputPressedFrameCounter <= 1
		&& CurrentTime >= JumpInputPressedAtSeconds
		&& CurrentTime - JumpInputPressedAtSeconds
			<= EverPlanetM1ManualProbe::JumpInputResponseTimeoutSeconds;

	bJumpActive = true;
	bJumpInputPending = false;
	bJumpAwaitingSpacePress = !bRecentSpacePress;
	bSpacePressSeenForCurrentJump = bRecentSpacePress;
	bJumpFallingSeen = false;
	bJumpOutwardSeen = false;
	bJumpApexSeen = false;
	bJumpDescendingSeen = false;
	bLandingEventSeen = false;
	LastLandedActor.Reset();
	JumpStartRadius = CenterRadius;
	JumpPeakRadius = CenterRadius;
	JumpStartedAtSeconds = CurrentTime;
	JumpStartedFrameCounter = CurrentFrameCounter;
	UE_LOG(
		LogEverPlanet,
		Display,
		TEXT("M1_MANUAL_JUMP_START checkpoint=%s radius=%.2f"),
		GetCheckpointName(),
		JumpStartRadius);
#endif
}

void AM1SphericalManualTestCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

#if !UE_BUILD_SHIPPING
	if (bManualProbeRuntimeEnabled && bJumpActive)
	{
		bLandingEventSeen = true;
		LastLandedActor = Hit.GetActor();
	}
#endif
}

void AM1SphericalManualTestCharacter::SelectNorthCheckpoint()
{
	ActivateCheckpoint(EManualCheckpoint::North);
}

void AM1SphericalManualTestCharacter::SelectEquatorCheckpoint()
{
	ActivateCheckpoint(EManualCheckpoint::Equator);
}

void AM1SphericalManualTestCharacter::SelectSouthCheckpoint()
{
	ActivateCheckpoint(EManualCheckpoint::South);
}

void AM1SphericalManualTestCharacter::SelectPoleCrossCheckpoint()
{
	ActivateCheckpoint(EManualCheckpoint::PoleCross);
}

void AM1SphericalManualTestCharacter::SelectFullLapCheckpoint()
{
	ActivateCheckpoint(EManualCheckpoint::FullLap);
}

void AM1SphericalManualTestCharacter::RestartCurrentCheckpoint()
{
	ActivateCheckpoint(CurrentCheckpoint);
}

void AM1SphericalManualTestCharacter::ToggleProbeHud()
{
	if (!bManualProbeRuntimeEnabled)
	{
		return;
	}

	bShowProbeHud = !bShowProbeHud;
	UE_LOG(
		LogEverPlanet,
		Display,
		TEXT("M1_MANUAL_HUD visible=%s"),
		bShowProbeHud ? TEXT("true") : TEXT("false"));
}

void AM1SphericalManualTestCharacter::OnManualJumpInputPressed()
{
	if (!bManualProbeRuntimeEnabled)
	{
		return;
	}

	const double CurrentTime = GetWorld()
		? static_cast<double>(GetWorld()->GetTimeSeconds())
		: 0.0;
	const uint64 CurrentFrameCounter = GFrameCounter;
	JumpInputPressedAtSeconds = CurrentTime;
	JumpInputPressedFrameCounter = CurrentFrameCounter;

	if (!bJumpActive)
	{
		bJumpInputPending = true;
		bJumpAwaitingSpacePress = false;
		bSpacePressSeenForCurrentJump = false;
		bJumpInputReleasedSeen = false;
	}
	else if (bJumpAwaitingSpacePress
		&& CurrentFrameCounter >= JumpStartedFrameCounter
		&& CurrentFrameCounter - JumpStartedFrameCounter <= 1
		&& CurrentTime >= JumpStartedAtSeconds
		&& CurrentTime - JumpStartedAtSeconds
			<= EverPlanetM1ManualProbe::JumpInputResponseTimeoutSeconds)
	{
		// Enhanced Input and raw key bindings can run in either order in the
		// same player-input frame. Accept only that tightly coupled ordering.
		bJumpInputPending = false;
		bJumpAwaitingSpacePress = false;
		bSpacePressSeenForCurrentJump = true;
		bJumpInputReleasedSeen = false;
	}
	UE_LOG(
		LogEverPlanet,
		Display,
		TEXT("M1_MANUAL_INPUT jump_pressed checkpoint=%s"),
		GetCheckpointName());
}

void AM1SphericalManualTestCharacter::OnManualJumpInputReleased()
{
	if (!bManualProbeRuntimeEnabled)
	{
		return;
	}

	bJumpInputReleasedSeen = true;
}

void AM1SphericalManualTestCharacter::ActivateCheckpoint(const EManualCheckpoint NewCheckpoint)
{
#if !UE_BUILD_SHIPPING
	if (!bManualProbeRuntimeEnabled)
	{
		return;
	}

	UPlanetGravityComponent* GravityComponent = GetPlanetGravityComponent();
	APlanetBody* Planet = GravityComponent ? GravityComponent->GetGravitySource() : nullptr;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!IsValid(Planet) || !Movement || !GetCapsuleComponent())
	{
		RecordProbeFailure(TEXT("checkpoint requested without a valid planet or movement component"));
		return;
	}

	FVector SurfaceNormal = FVector::ZeroVector;
	FVector TangentForward = FVector::ZeroVector;
	if (!TryGetCheckpointFrame(NewCheckpoint, SurfaceNormal, TangentForward))
	{
		RecordProbeFailure(TEXT("checkpoint frame could not be constructed"));
		return;
	}

	const double SpawnRadius = static_cast<double>(Planet->GetSurfaceRadius())
		+ static_cast<double>(GetCapsuleComponent()->GetScaledCapsuleHalfHeight())
		+ EverPlanetM1ManualProbe::SpawnClearance;
	const FVector TargetLocation = Planet->GetPlanetCenter() + SurfaceNormal * SpawnRadius;
	const FQuat TargetRotation = FRotationMatrix::MakeFromXZ(
		TangentForward,
		SurfaceNormal).ToQuat();
	const bool bTeleported = SetActorLocationAndRotation(
		TargetLocation,
		TargetRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	if (!bTeleported)
	{
		RecordProbeFailure(TEXT("checkpoint teleport failed"));
		return;
	}

	ConsumeMovementInputVector();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Falling);
	if (Controller)
	{
		Controller->SetControlRotation(TargetRotation.Rotator());
	}
	ResetPlanetMovementHeading(TangentForward);

	CurrentCheckpoint = NewCheckpoint;
	CheckpointPlanet = Planet;
	ResetCheckpointTelemetry(SurfaceNormal, TangentForward);
	UE_LOG(
		LogEverPlanet,
		Display,
		TEXT("M1_MANUAL_CHECKPOINT name=%s location=%s normal=%s forward=%s"),
		GetCheckpointName(),
		*TargetLocation.ToCompactString(),
		*SurfaceNormal.ToCompactString(),
		*TangentForward.ToCompactString());
#endif
}

bool AM1SphericalManualTestCharacter::TryGetCheckpointFrame(
	const EManualCheckpoint Checkpoint,
	FVector& OutSurfaceNormal,
	FVector& OutTangentForward) const
{
	OutSurfaceNormal = FVector::ZeroVector;
	OutTangentForward = FVector::ZeroVector;

	switch (Checkpoint)
	{
	case EManualCheckpoint::North:
		OutSurfaceNormal = FVector::UpVector;
		OutTangentForward = FVector::ForwardVector;
		break;
	case EManualCheckpoint::Equator:
		OutSurfaceNormal = FVector::ForwardVector;
		OutTangentForward = FVector::RightVector;
		break;
	case EManualCheckpoint::South:
		OutSurfaceNormal = FVector::DownVector;
		OutTangentForward = FVector::ForwardVector;
		break;
	case EManualCheckpoint::PoleCross:
	{
		const double Radians = FMath::DegreesToRadians(EverPlanetM1ManualProbe::PoleStartDegrees);
		OutSurfaceNormal = FVector(FMath::Sin(Radians), 0.0, FMath::Cos(Radians));
		OutTangentForward = FPlanetGravityMath::ProjectOntoTangentPlane(
			FVector::UpVector,
			OutSurfaceNormal);
		break;
	}
	case EManualCheckpoint::FullLap:
		OutSurfaceNormal = FVector::ForwardVector;
		OutTangentForward = FVector::RightVector;
		break;
	default:
		return false;
	}

	return OutSurfaceNormal.Normalize()
		&& OutTangentForward.Normalize()
		&& FMath::Abs(FVector::DotProduct(OutSurfaceNormal, OutTangentForward)) <= 1.0e-4;
}

bool AM1SphericalManualTestCharacter::TryGetPlanetFrame(
	FVector& OutSurfaceNormal,
	FVector& OutGravityDirection,
	double& OutCenterRadius,
	double& OutExpectedGroundRadius) const
{
	OutSurfaceNormal = FVector::ZeroVector;
	OutGravityDirection = FVector::ZeroVector;
	OutCenterRadius = 0.0;
	OutExpectedGroundRadius = 0.0;

	const UPlanetGravityComponent* GravityComponent = GetPlanetGravityComponent();
	const APlanetBody* Planet = GravityComponent ? GravityComponent->GetGravitySource() : nullptr;
	if (!IsValid(Planet)
		|| !GetCapsuleComponent()
		|| !GravityComponent->TryGetSurfaceNormal(OutSurfaceNormal)
		|| !GravityComponent->TryGetGravityDirection(OutGravityDirection))
	{
		return false;
	}

	OutCenterRadius = FVector::Distance(GetActorLocation(), Planet->GetPlanetCenter());
	OutExpectedGroundRadius = static_cast<double>(Planet->GetSurfaceRadius())
		+ static_cast<double>(GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	return EverPlanetM1ManualProbe::IsFiniteVector(OutSurfaceNormal)
		&& EverPlanetM1ManualProbe::IsFiniteVector(OutGravityDirection)
		&& FMath::IsFinite(OutCenterRadius)
		&& FMath::IsFinite(OutExpectedGroundRadius);
}

void AM1SphericalManualTestCharacter::ResetCheckpointTelemetry(
	const FVector& StartNormal,
	const FVector& AuthoredTangentForward)
{
	CheckpointStartNormal = StartNormal;
	CheckpointTravelAxis = FVector::CrossProduct(
		StartNormal,
		AuthoredTangentForward).GetSafeNormal();
	PreviousSurfaceNormal = StartNormal;
	AccumulatedSurfaceTravelDegrees = 0.0;
	MaximumSurfaceZ = StartNormal.Z;
	MaximumGreatCircleDriftDegrees = 0.0;
	LapCheckpointMask = 0;
	LastMoveAxes = FVector::ZeroVector;
	LastLookAxes = FVector::ZeroVector;
	bMoveForwardSeen = false;
	bMoveBackwardSeen = false;
	bMoveRightSeen = false;
	bMoveLeftSeen = false;
	bDiagonalMoveSeen = false;
	bLookYawSeen = false;
	bLookPitchSeen = false;
	bJumpInputPending = false;
	bJumpAwaitingSpacePress = false;
	bSpacePressSeenForCurrentJump = false;
	bJumpInputReleasedSeen = false;
	bJumpActive = false;
	bJumpFallingSeen = false;
	bJumpOutwardSeen = false;
	bJumpApexSeen = false;
	bJumpDescendingSeen = false;
	bLandingEventSeen = false;
	bTraversalGroundIntegrityArmed = false;
	bPolePassLogged = false;
	bLapPassLogged = false;
	bProbeFailureLogged = false;
	LastLandedActor.Reset();
	FirstProbeFailure.Reset();
}

void AM1SphericalManualTestCharacter::UpdateProbeTelemetry(const float DeltaSeconds)
{
	FVector SurfaceNormal = FVector::ZeroVector;
	FVector GravityDirection = FVector::ZeroVector;
	double CenterRadius = 0.0;
	double ExpectedGroundRadius = 0.0;
	if (!TryGetPlanetFrame(
		SurfaceNormal,
		GravityDirection,
		CenterRadius,
		ExpectedGroundRadius))
	{
		RecordProbeFailure(TEXT("planet frame unavailable during probe tick"));
		return;
	}

	const double SurfaceDot = FMath::Clamp(
		static_cast<double>(FVector::DotProduct(PreviousSurfaceNormal, SurfaceNormal)),
		-1.0,
		1.0);
	const double SignedFrameTravelDegrees = FMath::RadiansToDegrees(FMath::Atan2(
		static_cast<double>(FVector::DotProduct(
			CheckpointTravelAxis,
			FVector::CrossProduct(PreviousSurfaceNormal, SurfaceNormal))),
		SurfaceDot));
	PreviousSurfaceNormal = SurfaceNormal;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const UPlanetGravityComponent* GravityComponent = GetPlanetGravityComponent();
	const APlanetBody* Planet = GravityComponent ? GravityComponent->GetGravitySource() : nullptr;
	const double RadialSpeed = Movement
		? static_cast<double>(FVector::DotProduct(Movement->Velocity, SurfaceNormal))
		: 0.0;

	const bool bFinite = EverPlanetM1ManualProbe::IsFiniteVector(GetActorLocation())
		&& EverPlanetM1ManualProbe::IsFiniteVector(GetActorUpVector())
		&& Movement
		&& EverPlanetM1ManualProbe::IsFiniteVector(Movement->Velocity);
	const double UpDot = FVector::DotProduct(GetActorUpVector(), SurfaceNormal);
	const double GravityDot = Movement
		? FMath::Clamp(
			static_cast<double>(FVector::DotProduct(
				Movement->GetGravityDirection(),
				GravityDirection)),
			-1.0,
			1.0)
		: -1.0;
	const double GravityErrorDegrees = FMath::RadiansToDegrees(FMath::Acos(GravityDot));
	const double RadiusDrift = FMath::Abs(CenterRadius - ExpectedGroundRadius);
	const bool bGroundedOnCheckpointPlanet = Movement
		&& Movement->IsMovingOnGround()
		&& Movement->CurrentFloor.IsWalkableFloor()
		&& Movement->CurrentFloor.HitResult.GetActor() == Planet
		&& Planet == CheckpointPlanet.Get();
	const bool bTraversalCheckpoint = CurrentCheckpoint == EManualCheckpoint::PoleCross
		|| CurrentCheckpoint == EManualCheckpoint::FullLap;
	if (bTraversalCheckpoint && bMoveForwardSeen && bGroundedOnCheckpointPlanet)
	{
		bTraversalGroundIntegrityArmed = true;
	}
	if (bTraversalGroundIntegrityArmed && !bGroundedOnCheckpointPlanet)
	{
		RecordProbeFailure(TEXT("surface traversal left the authored PlanetBody floor"));
	}

	const bool bValidFrameTravel = FMath::IsFinite(SignedFrameTravelDegrees)
		&& FMath::Abs(SignedFrameTravelDegrees) <= 45.0;
	if (bValidFrameTravel && (!bTraversalCheckpoint || bTraversalGroundIntegrityArmed))
	{
		AccumulatedSurfaceTravelDegrees += SignedFrameTravelDegrees;
		if (bTraversalCheckpoint
			&& LastMoveAxes.Y > EverPlanetM1ManualProbe::InputThreshold
			&& SignedFrameTravelDegrees < -0.5)
		{
			RecordProbeFailure(TEXT("forward input reversed along the authored great-circle axis"));
		}
	}
	if (!bTraversalCheckpoint || bTraversalGroundIntegrityArmed)
	{
		MaximumSurfaceZ = FMath::Max(MaximumSurfaceZ, static_cast<double>(SurfaceNormal.Z));
		if (!CheckpointTravelAxis.IsNearlyZero())
		{
			const double PlaneOffset = FMath::Clamp(
				FMath::Abs(static_cast<double>(FVector::DotProduct(
					SurfaceNormal,
					CheckpointTravelAxis))),
				0.0,
				1.0);
			MaximumGreatCircleDriftDegrees = FMath::Max(
				MaximumGreatCircleDriftDegrees,
				FMath::RadiansToDegrees(FMath::Asin(PlaneOffset)));
		}
	}
	if (Movement && Movement->IsMovingOnGround())
	{
		if (!bFinite)
		{
			RecordProbeFailure(TEXT("non-finite transform or velocity while grounded"));
		}
		else if (UpDot < EverPlanetM1ManualProbe::AlignmentDotThreshold)
		{
			RecordProbeFailure(TEXT("capsule local up diverged from the planet normal"));
		}
		else if (GravityErrorDegrees > EverPlanetM1ManualProbe::GravityErrorDegreesThreshold)
		{
			RecordProbeFailure(TEXT("CharacterMovement gravity diverged from the planet frame"));
		}
		else if (RadiusDrift > EverPlanetM1ManualProbe::GroundRadiusTolerance)
		{
			RecordProbeFailure(TEXT("grounded radial drift exceeded 25 cm"));
		}
		else if (!bGroundedOnCheckpointPlanet)
		{
			RecordProbeFailure(TEXT("grounded floor or gravity source changed from the checkpoint planet"));
		}
	}
	if (bGroundedOnCheckpointPlanet
		&& bMoveForwardSeen
		&& (CurrentCheckpoint == EManualCheckpoint::PoleCross
			|| CurrentCheckpoint == EManualCheckpoint::FullLap)
		&& MaximumGreatCircleDriftDegrees
			> EverPlanetM1ManualProbe::GreatCircleDriftDegreesThreshold)
	{
		RecordProbeFailure(TEXT("surface path drifted more than 5 degrees from the authored great circle"));
	}

	const double CurrentTime = GetWorld()
		? static_cast<double>(GetWorld()->GetTimeSeconds())
		: 0.0;
	if (bJumpInputPending
		&& !bJumpActive
		&& CurrentTime - JumpInputPressedAtSeconds
			> EverPlanetM1ManualProbe::JumpInputResponseTimeoutSeconds)
	{
		RecordProbeFailure(TEXT("Space was received but no character jump started within 0.75 seconds"));
		bJumpInputPending = false;
	}
	if (bJumpAwaitingSpacePress
		&& CurrentTime - JumpStartedAtSeconds
			> EverPlanetM1ManualProbe::JumpInputResponseTimeoutSeconds)
	{
		RecordProbeFailure(TEXT("character jump started without a matching Space input frame"));
		bJumpAwaitingSpacePress = false;
	}

	UpdateJumpTelemetry(SurfaceNormal, CenterRadius, RadialSpeed);
	UpdateLapCheckpointProgress(SurfaceNormal);

	const bool bStablePassFrame = !bProbeFailureLogged
		&& bGroundedOnCheckpointPlanet
		&& bFinite
		&& UpDot >= EverPlanetM1ManualProbe::AlignmentDotThreshold
		&& GravityErrorDegrees <= EverPlanetM1ManualProbe::GravityErrorDegreesThreshold
		&& RadiusDrift <= EverPlanetM1ManualProbe::GroundRadiusTolerance;

	if (CurrentCheckpoint == EManualCheckpoint::PoleCross
		&& !bPolePassLogged
		&& bStablePassFrame
		&& bMoveForwardSeen
		&& AccumulatedSurfaceTravelDegrees >= 30.0
		&& MaximumSurfaceZ >= 0.999
		&& MaximumGreatCircleDriftDegrees
			<= EverPlanetM1ManualProbe::GreatCircleDriftDegreesThreshold)
	{
		bPolePassLogged = true;
		UE_LOG(
			LogEverPlanet,
			Display,
			TEXT("M1_MANUAL_POLE_PASS travelDegrees=%.2f maxSurfaceZ=%.5f maxPlaneDrift=%.2f"),
			AccumulatedSurfaceTravelDegrees,
			MaximumSurfaceZ,
			MaximumGreatCircleDriftDegrees);
	}

	const double ReturnDot = FMath::Clamp(
		static_cast<double>(FVector::DotProduct(CheckpointStartNormal, SurfaceNormal)),
		-1.0,
		1.0);
	const double ReturnAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(ReturnDot));
	if (CurrentCheckpoint == EManualCheckpoint::FullLap
		&& !bLapPassLogged
		&& bStablePassFrame
		&& bMoveForwardSeen
		&& LapCheckpointMask == 0x07
		&& AccumulatedSurfaceTravelDegrees >= 350.0
		&& ReturnAngleDegrees <= 15.0
		&& MaximumGreatCircleDriftDegrees
			<= EverPlanetM1ManualProbe::GreatCircleDriftDegreesThreshold)
	{
		bLapPassLogged = true;
		UE_LOG(
			LogEverPlanet,
			Display,
			TEXT("M1_MANUAL_LAP_PASS travelDegrees=%.2f returnAngle=%.2f checkpoints=0x%02x maxPlaneDrift=%.2f"),
			AccumulatedSurfaceTravelDegrees,
			ReturnAngleDegrees,
			LapCheckpointMask,
			MaximumGreatCircleDriftDegrees);
	}

	if (bShowProbeHud)
	{
		DrawProbeHud(
			SurfaceNormal,
			GravityDirection,
			CenterRadius,
			ExpectedGroundRadius,
			RadialSpeed);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			EverPlanetM1ManualProbe::HudMessageKey,
			0.01f,
			FColor::Transparent,
			FString());
	}

	(void)DeltaSeconds;
}

void AM1SphericalManualTestCharacter::UpdateLapCheckpointProgress(
	const FVector& SurfaceNormal)
{
	if (CurrentCheckpoint != EManualCheckpoint::FullLap
		|| CheckpointTravelAxis.IsNearlyZero())
	{
		return;
	}

	static constexpr double CheckpointDegrees[] = {90.0, 180.0, 270.0};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheckpointDegrees); ++Index)
	{
		const uint8 Bit = static_cast<uint8>(1u << Index);
		const double TargetDegrees = CheckpointDegrees[Index];
		if ((LapCheckpointMask & Bit) != 0
			|| AccumulatedSurfaceTravelDegrees < TargetDegrees)
		{
			continue;
		}

		const FVector ExpectedNormal = FQuat(
			CheckpointTravelAxis,
			FMath::DegreesToRadians(TargetDegrees)).RotateVector(CheckpointStartNormal);
		const double ExpectedDot = FMath::Clamp(
			static_cast<double>(FVector::DotProduct(ExpectedNormal, SurfaceNormal)),
			-1.0,
			1.0);
		const double CheckpointErrorDegrees = FMath::RadiansToDegrees(FMath::Acos(ExpectedDot));
		LapCheckpointMask |= Bit;
		UE_LOG(
			LogEverPlanet,
			Display,
			TEXT("M1_MANUAL_LAP_CHECKPOINT target=%.0f error=%.2f mask=0x%02x"),
			TargetDegrees,
			CheckpointErrorDegrees,
			LapCheckpointMask);
		if (CheckpointErrorDegrees
			> EverPlanetM1ManualProbe::GreatCircleDriftDegreesThreshold)
		{
			RecordProbeFailure(
				FString::Printf(
					TEXT("full-lap %.0f degree checkpoint deviated by %.2f degrees"),
					TargetDegrees,
					CheckpointErrorDegrees));
		}
	}
}

void AM1SphericalManualTestCharacter::UpdateJumpTelemetry(
	const FVector& SurfaceNormal,
	const double CenterRadius,
	const double RadialSpeed)
{
	if (!bJumpActive)
	{
		return;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	JumpPeakRadius = FMath::Max(JumpPeakRadius, CenterRadius);
	if (Movement && Movement->IsFalling())
	{
		bJumpFallingSeen = true;
	}
	if (bJumpFallingSeen && RadialSpeed > 20.0)
	{
		bJumpOutwardSeen = true;
	}
	if (bJumpOutwardSeen && RadialSpeed <= 0.0)
	{
		bJumpApexSeen = true;
	}
	if (bJumpApexSeen && RadialSpeed < -20.0)
	{
		bJumpDescendingSeen = true;
	}

	const double CurrentTime = GetWorld() ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
	if (CurrentTime - JumpStartedAtSeconds > EverPlanetM1ManualProbe::JumpTimeoutSeconds)
	{
		RecordProbeFailure(TEXT("jump did not return to the planet within 8 seconds"));
		bJumpActive = false;
		return;
	}

	if (bLandingEventSeen && Movement && Movement->IsMovingOnGround())
	{
		const UPlanetGravityComponent* GravityComponent = GetPlanetGravityComponent();
		const APlanetBody* Planet = GravityComponent ? GravityComponent->GetGravitySource() : nullptr;
		const double ExpectedGroundRadius = Planet && GetCapsuleComponent()
			? static_cast<double>(Planet->GetSurfaceRadius())
				+ static_cast<double>(GetCapsuleComponent()->GetScaledCapsuleHalfHeight())
			: 0.0;
		const bool bPhysicsPass = bJumpFallingSeen
			&& bJumpOutwardSeen
			&& bJumpApexSeen
			&& bJumpDescendingSeen
			&& JumpPeakRadius - JumpStartRadius >= EverPlanetM1ManualProbe::MinimumJumpHeight
			&& Planet == CheckpointPlanet.Get()
			&& LastLandedActor.Get() == Planet
			&& Movement->CurrentFloor.IsWalkableFloor()
			&& Movement->CurrentFloor.HitResult.GetActor() == Planet
			&& FMath::Abs(CenterRadius - ExpectedGroundRadius)
				<= EverPlanetM1ManualProbe::GroundRadiusTolerance;
		if (!bPhysicsPass && !bProbeFailureLogged)
		{
			RecordProbeFailure(
				FString::Printf(
					TEXT("jump sequence incomplete space=%d released=%d falling=%d outward=%d apex=%d descending=%d landedOnPlanet=%d"),
					bSpacePressSeenForCurrentJump,
					bJumpInputReleasedSeen,
					bJumpFallingSeen,
					bJumpOutwardSeen,
					bJumpApexSeen,
					bJumpDescendingSeen,
					LastLandedActor.Get() == Planet));
		}
		if (!bProbeFailureLogged
			&& bPhysicsPass
			&& bSpacePressSeenForCurrentJump
			&& bJumpInputReleasedSeen)
		{
			UE_LOG(
				LogEverPlanet,
				Display,
				TEXT("M1_MANUAL_JUMP_PASS checkpoint=%s startRadius=%.2f peakRadius=%.2f landedRadius=%.2f"),
				GetCheckpointName(),
				JumpStartRadius,
				JumpPeakRadius,
				CenterRadius);
			bJumpActive = false;
		}
		else if (bProbeFailureLogged)
		{
			bJumpActive = false;
		}
	}

	(void)SurfaceNormal;
}

void AM1SphericalManualTestCharacter::DrawProbeHud(
	const FVector& SurfaceNormal,
	const FVector& GravityDirection,
	const double CenterRadius,
	const double ExpectedGroundRadius,
	const double RadialSpeed) const
{
	if (!GEngine)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const UPlanetGravityComponent* GravityComponent = GetPlanetGravityComponent();
	const APlanetBody* Planet = GravityComponent ? GravityComponent->GetGravitySource() : nullptr;
	const AActor* FloorActor = Movement ? Movement->CurrentFloor.HitResult.GetActor() : nullptr;
	const double UpDot = FVector::DotProduct(GetActorUpVector(), SurfaceNormal);
	const double GravityDot = Movement
		? FMath::Clamp(
			static_cast<double>(FVector::DotProduct(
				Movement->GetGravityDirection(),
				GravityDirection)),
			-1.0,
			1.0)
		: -1.0;
	const double GravityErrorDegrees = FMath::RadiansToDegrees(FMath::Acos(GravityDot));
	const double ReturnDot = FMath::Clamp(
		static_cast<double>(FVector::DotProduct(CheckpointStartNormal, SurfaceNormal)),
		-1.0,
		1.0);
	const double ReturnAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(ReturnDot));
	const FString HudText = FString::Printf(
		TEXT("M1 MANUAL PROBE - TEST ONLY / NOT M2 CAMERA\n")
		TEXT("Focus viewport | F1 North  F2 Equator  F3 South  F4 Pole  F5 Lap  R Restart  H HUD\n")
		TEXT("Checkpoint=%s  State=%s  Planet=%s  Floor=%s\n")
		TEXT("Radius=%.2f  Ground=%.2f  Drift=%.2f  RadialSpeed=%.2f\n")
		TEXT("Up.Normal=%.5f  GravityError=%.4f deg  SurfaceNormal=%s\n")
		TEXT("Move last=(R %.2f,F %.2f) seen[F%d B%d R%d L%d D%d]  Look=(Y %.2f,P %.2f) seen[Y%d P%d]\n")
		TEXT("Jump pending=%d awaitingSpace=%d space=%d released=%d active=%d falling=%d outward=%d apex=%d descending=%d height=%.2f\n")
		TEXT("SignedTravel=%.2f deg  ReturnAngle=%.2f  PlaneDriftMax=%.2f  LapCP=0x%02x  PolePass=%d  LapPass=%d\n")
		TEXT("Probe=%s%s"),
		GetCheckpointName(),
		Movement && Movement->IsMovingOnGround() ? TEXT("Grounded") : TEXT("Falling"),
		*GetNameSafe(Planet),
		*GetNameSafe(FloorActor),
		CenterRadius,
		ExpectedGroundRadius,
		FMath::Abs(CenterRadius - ExpectedGroundRadius),
		RadialSpeed,
		UpDot,
		GravityErrorDegrees,
		*SurfaceNormal.ToCompactString(),
		LastMoveAxes.X,
		LastMoveAxes.Y,
		bMoveForwardSeen,
		bMoveBackwardSeen,
		bMoveRightSeen,
		bMoveLeftSeen,
		bDiagonalMoveSeen,
		LastLookAxes.X,
		LastLookAxes.Y,
		bLookYawSeen,
		bLookPitchSeen,
		bJumpInputPending,
		bJumpAwaitingSpacePress,
		bSpacePressSeenForCurrentJump,
		bJumpInputReleasedSeen,
		bJumpActive,
		bJumpFallingSeen,
		bJumpOutwardSeen,
		bJumpApexSeen,
		bJumpDescendingSeen,
		JumpPeakRadius - JumpStartRadius,
		AccumulatedSurfaceTravelDegrees,
		ReturnAngleDegrees,
		MaximumGreatCircleDriftDegrees,
		LapCheckpointMask,
		bPolePassLogged,
		bLapPassLogged,
		bProbeFailureLogged ? TEXT("FAIL: ") : TEXT("WAIT/PASS"),
		bProbeFailureLogged ? *FirstProbeFailure : TEXT(""));

	GEngine->AddOnScreenDebugMessage(
		EverPlanetM1ManualProbe::HudMessageKey,
		0.1f,
		bProbeFailureLogged ? FColor::Red : FColor::Cyan,
		HudText,
		false,
		FVector2D(0.9f, 0.9f));
}

void AM1SphericalManualTestCharacter::RecordProbeFailure(const FString& Reason)
{
	if (bProbeFailureLogged)
	{
		return;
	}

	bProbeFailureLogged = true;
	FirstProbeFailure = Reason;
	UE_LOG(
		LogEverPlanet,
		Error,
		TEXT("M1_MANUAL_FAIL checkpoint=%s reason=%s"),
		GetCheckpointName(),
		*Reason);
}

const TCHAR* AM1SphericalManualTestCharacter::GetCheckpointName() const
{
	switch (CurrentCheckpoint)
	{
	case EManualCheckpoint::North:
		return TEXT("North");
	case EManualCheckpoint::Equator:
		return TEXT("Equator");
	case EManualCheckpoint::South:
		return TEXT("South");
	case EManualCheckpoint::PoleCross:
		return TEXT("PoleCross");
	case EManualCheckpoint::FullLap:
		return TEXT("FullLap");
	default:
		return TEXT("Unknown");
	}
}

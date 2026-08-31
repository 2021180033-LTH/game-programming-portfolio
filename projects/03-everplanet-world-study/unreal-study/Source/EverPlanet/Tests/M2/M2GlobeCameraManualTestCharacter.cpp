// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/M2/M2GlobeCameraManualTestCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlanetGlobeCameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Math/RotationMatrix.h"
#include "Misc/App.h"

#include "EverPlanet.h"
#include "Planet/PlanetBody.h"
#include "Planet/PlanetGravityComponent.h"
#include "Planet/PlanetGravityMath.h"

namespace EverPlanetM2CameraProbe
{
	constexpr int32 HudMessageKey = 0x4D324341;
	constexpr double SpawnClearance = 25.0;
	constexpr double PoleStartDegrees = 15.0;

	double AngleBetweenDegrees(const FVector& A, const FVector& B)
	{
		const double Dot = FMath::Clamp(
			static_cast<double>(FVector::DotProduct(A, B)),
			-1.0,
			1.0);
		return FMath::RadiansToDegrees(FMath::Acos(Dot));
	}
}

AM2GlobeCameraManualTestCharacter::AM2GlobeCameraManualTestCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bManualCameraProbeEnabled = FApp::CanEverRender()
		&& !FApp::IsUnattended()
		&& !GIsAutomationTesting;
}

void AM2GlobeCameraManualTestCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Evaluate the runtime environment again on the spawned instance.  A class
	// default object can be constructed before the editor's render state is final,
	// so preserving a constructor-time false value would silently disable the
	// attended PIE probe forever.
	bManualCameraProbeEnabled = FApp::CanEverRender()
		&& !FApp::IsUnattended()
		&& !GIsAutomationTesting;
}

void AM2GlobeCameraManualTestCharacter::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
#if !UE_BUILD_SHIPPING
	if (bManualCameraProbeEnabled && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			EverPlanetM2CameraProbe::HudMessageKey,
			0.01f,
			FColor::Transparent,
			FString());
	}
#endif

	Super::EndPlay(EndPlayReason);
}

void AM2GlobeCameraManualTestCharacter::SetupPlayerInputComponent(
	UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

#if !UE_BUILD_SHIPPING
	const bool bRuntimeAllowsManualProbe = FApp::CanEverRender()
		&& !FApp::IsUnattended()
		&& !GIsAutomationTesting;
	if (!PlayerInputComponent
		|| !bManualCameraProbeEnabled
		|| !bRuntimeAllowsManualProbe)
	{
		return;
	}

	auto BindProbeKey = [this, PlayerInputComponent](
		const FKey Key,
		void (AM2GlobeCameraManualTestCharacter::*Function)())
	{
		FInputKeyBinding& Binding = PlayerInputComponent->BindKey(
			Key,
			IE_Pressed,
			this,
			Function);
		Binding.bConsumeInput = false;
	};

	BindProbeKey(EKeys::F1, &AM2GlobeCameraManualTestCharacter::SelectNorthCheckpoint);
	BindProbeKey(EKeys::F2, &AM2GlobeCameraManualTestCharacter::SelectEquatorCheckpoint);
	BindProbeKey(EKeys::F3, &AM2GlobeCameraManualTestCharacter::SelectSouthCheckpoint);
	BindProbeKey(EKeys::F4, &AM2GlobeCameraManualTestCharacter::SelectPoleCrossCheckpoint);
	BindProbeKey(EKeys::F5, &AM2GlobeCameraManualTestCharacter::SelectFullLapCheckpoint);
	BindProbeKey(EKeys::F6, &AM2GlobeCameraManualTestCharacter::SelectReferencePreset);
	BindProbeKey(EKeys::F7, &AM2GlobeCameraManualTestCharacter::SelectSteeperPreset);
	BindProbeKey(EKeys::F8, &AM2GlobeCameraManualTestCharacter::SelectWidePreset);
	BindProbeKey(EKeys::R, &AM2GlobeCameraManualTestCharacter::RestartCurrentCheckpoint);
	BindProbeKey(EKeys::H, &AM2GlobeCameraManualTestCharacter::ToggleCameraHud);

	UE_LOG(
		LogEverPlanet,
		Display,
		TEXT("M2_CAMERA_READY pawn=%s checkpoints=F1/F2/F3/F4/F5 presets=F6/F7/F8 restart=R hud=H viewport_focus_required=true"),
		*GetNameSafe(this));
#endif
}

void AM2GlobeCameraManualTestCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if !UE_BUILD_SHIPPING
	if (bManualCameraProbeEnabled && IsLocallyControlled() && bShowCameraHud)
	{
		DrawCameraHud();
	}
#endif
}

void AM2GlobeCameraManualTestCharacter::SelectNorthCheckpoint()
{
	ActivateCheckpoint(ECameraCheckpoint::North);
}

void AM2GlobeCameraManualTestCharacter::SelectEquatorCheckpoint()
{
	ActivateCheckpoint(ECameraCheckpoint::Equator);
}

void AM2GlobeCameraManualTestCharacter::SelectSouthCheckpoint()
{
	ActivateCheckpoint(ECameraCheckpoint::South);
}

void AM2GlobeCameraManualTestCharacter::SelectPoleCrossCheckpoint()
{
	ActivateCheckpoint(ECameraCheckpoint::PoleCross);
}

void AM2GlobeCameraManualTestCharacter::SelectFullLapCheckpoint()
{
	ActivateCheckpoint(ECameraCheckpoint::FullLap);
}

void AM2GlobeCameraManualTestCharacter::RestartCurrentCheckpoint()
{
	ActivateCheckpoint(CurrentCheckpoint);
}

void AM2GlobeCameraManualTestCharacter::SelectReferencePreset()
{
	ApplyPreset(ECameraPreset::Reference);
}

void AM2GlobeCameraManualTestCharacter::SelectSteeperPreset()
{
	ApplyPreset(ECameraPreset::Steeper);
}

void AM2GlobeCameraManualTestCharacter::SelectWidePreset()
{
	ApplyPreset(ECameraPreset::Wide);
}

void AM2GlobeCameraManualTestCharacter::ToggleCameraHud()
{
	bShowCameraHud = !bShowCameraHud;
	if (!bShowCameraHud && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			EverPlanetM2CameraProbe::HudMessageKey,
			0.01f,
			FColor::Transparent,
			FString());
	}
}

void AM2GlobeCameraManualTestCharacter::ActivateCheckpoint(
	const ECameraCheckpoint NewCheckpoint)
{
	UPlanetGravityComponent* GravityComponent = GetPlanetGravityComponent();
	APlanetBody* Planet = GravityComponent ? GravityComponent->GetGravitySource() : nullptr;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	FVector SurfaceNormal = FVector::ZeroVector;
	FVector TangentForward = FVector::ZeroVector;
	if (!IsValid(Planet)
		|| !Movement
		|| !Capsule
		|| !TryGetCheckpointFrame(NewCheckpoint, SurfaceNormal, TangentForward))
	{
		UE_LOG(
			LogEverPlanet,
			Warning,
			TEXT("M2_CAMERA_CHECKPOINT_REJECTED name=%s reason=planet_or_frame_unavailable"),
			GetCheckpointName(NewCheckpoint));
		return;
	}

	const double TargetCenterRadius = static_cast<double>(Planet->GetSurfaceRadius())
		+ static_cast<double>(Capsule->GetScaledCapsuleHalfHeight())
		+ EverPlanetM2CameraProbe::SpawnClearance;
	const FVector TargetLocation = Planet->GetPlanetCenter()
		+ SurfaceNormal * TargetCenterRadius;
	const FQuat TargetRotation = FRotationMatrix::MakeFromXZ(
		TangentForward,
		SurfaceNormal).ToQuat();

	Movement->StopMovementImmediately();
	ConsumeMovementInputVector();
	const bool bTeleported = SetActorLocationAndRotation(
		TargetLocation,
		TargetRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	if (!bTeleported)
	{
		UE_LOG(
			LogEverPlanet,
			Warning,
			TEXT("M2_CAMERA_CHECKPOINT_REJECTED name=%s reason=teleport_failed"),
			GetCheckpointName(NewCheckpoint));
		return;
	}

	CurrentCheckpoint = NewCheckpoint;
	Movement->SetMovementMode(MOVE_Falling);
	ResetGlobeCameraHeading(TangentForward, true);

	UE_LOG(
		LogEverPlanet,
		Display,
		TEXT("M2_CAMERA_CHECKPOINT name=%s location=%s normal=%s forward=%s"),
		GetCheckpointName(),
		*TargetLocation.ToCompactString(),
		*SurfaceNormal.ToCompactString(),
		*TangentForward.ToCompactString());
}

void AM2GlobeCameraManualTestCharacter::ApplyPreset(
	const ECameraPreset NewPreset)
{
	UPlanetGlobeCameraComponent* CameraRig = GetPlanetGlobeCameraComponent();
	if (!CameraRig)
	{
		return;
	}

	float ArmLength = 1600.0f;
	float DownPitchDegrees = 38.0f;
	if (NewPreset == ECameraPreset::Steeper)
	{
		DownPitchDegrees = 43.0f;
	}
	else if (NewPreset == ECameraPreset::Wide)
	{
		ArmLength = 2000.0f;
	}

	const bool bApplied = CameraRig->ApplyCameraTuning(
		ArmLength,
		DownPitchDegrees,
		60.0f,
		100.0f,
		0.0f,
		true);
	if (bApplied)
	{
		CurrentPreset = NewPreset;
	}
	UE_LOG(
		LogEverPlanet,
		Display,
		TEXT("M2_CAMERA_PRESET name=%s applied=%s arm=%.1f pitch=%.1f hfov=60.0 upOffset=100.0 lookAhead=0.0"),
		GetPresetName(),
		bApplied ? TEXT("true") : TEXT("false"),
		ArmLength,
		DownPitchDegrees);
}

bool AM2GlobeCameraManualTestCharacter::TryGetCheckpointFrame(
	const ECameraCheckpoint Checkpoint,
	FVector& OutSurfaceNormal,
	FVector& OutTangentForward) const
{
	switch (Checkpoint)
	{
	case ECameraCheckpoint::North:
		OutSurfaceNormal = FVector::UpVector;
		OutTangentForward = FVector::ForwardVector;
		break;
	case ECameraCheckpoint::Equator:
		OutSurfaceNormal = FVector::ForwardVector;
		OutTangentForward = FVector::RightVector;
		break;
	case ECameraCheckpoint::South:
		OutSurfaceNormal = -FVector::UpVector;
		OutTangentForward = FVector::ForwardVector;
		break;
	case ECameraCheckpoint::PoleCross:
	{
		const double Radians = FMath::DegreesToRadians(
			EverPlanetM2CameraProbe::PoleStartDegrees);
		OutSurfaceNormal = FVector(
			FMath::Sin(Radians),
			0.0,
			FMath::Cos(Radians));
		OutTangentForward = FPlanetGravityMath::ProjectOntoTangentPlane(
			FVector::UpVector,
			OutSurfaceNormal);
		break;
	}
	case ECameraCheckpoint::FullLap:
		OutSurfaceNormal = FVector::ForwardVector;
		OutTangentForward = FVector::RightVector;
		break;
	default:
		return false;
	}

	return OutSurfaceNormal.Normalize()
		&& OutTangentForward.Normalize()
		&& FMath::Abs(FVector::DotProduct(
			OutSurfaceNormal,
			OutTangentForward)) <= 1.0e-4;
}

void AM2GlobeCameraManualTestCharacter::DrawCameraHud() const
{
	if (!GEngine)
	{
		return;
	}

	const UPlanetGravityComponent* GravityComponent = GetPlanetGravityComponent();
	const UPlanetGlobeCameraComponent* CameraRig = GetPlanetGlobeCameraComponent();
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const USpringArmComponent* Boom = GetCameraBoom();
	const UCameraComponent* Camera = GetFollowCamera();
	const APlanetBody* Planet = GravityComponent ? GravityComponent->GetGravitySource() : nullptr;
	FVector LocalUp = FVector::ZeroVector;
	const bool bHasSurfaceNormal = GravityComponent
		&& GravityComponent->TryGetSurfaceNormal(LocalUp);
	const FVector ViewForward = Camera ? Camera->GetForwardVector() : FVector::ZeroVector;
	const FVector ViewUp = Camera ? Camera->GetUpVector() : FVector::ZeroVector;
	const FVector ProjectedLocalUp = bHasSurfaceNormal
		? FVector::VectorPlaneProject(LocalUp, ViewForward).GetSafeNormal()
		: FVector::ZeroVector;
	const double ActualDownPitch = bHasSurfaceNormal
		? FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(
			-static_cast<double>(FVector::DotProduct(ViewForward, LocalUp)),
			-1.0,
			1.0)))
		: 0.0;
	const double RollError = !ProjectedLocalUp.IsNearlyZero() && !ViewUp.IsNearlyZero()
		? EverPlanetM2CameraProbe::AngleBetweenDegrees(ProjectedLocalUp, ViewUp)
		: 180.0;
	const FVector EffectivePivot = Boom
		? Boom->GetComponentLocation() + Boom->TargetOffset
		: FVector::ZeroVector;
	const double ActualArmLength = Boom && Camera
		? FVector::Distance(EffectivePivot, Camera->GetComponentLocation())
		: 0.0;
	const bool bFramePass = bHasSurfaceNormal
		&& CameraRig
		&& Movement
		&& Movement->IsMovingOnGround()
		&& RollError <= 1.0
		&& FMath::Abs(
			ActualDownPitch - CameraRig->GetCameraDownPitchDegrees()) <= 1.0;

	const FString HudText = FString::Printf(
		TEXT("M2 CAMERA PROBE - TEST ONLY / LOCAL-UP QUARTER VIEW\n")
		TEXT("F1 North F2 Equator F3 South F4 Pole F5 Lap | F6 Reference F7 Steeper F8 Wide | R Restart H HUD\n")
		TEXT("Checkpoint=%s Preset=%s State=%s Planet=%s\n")
		TEXT("Pitch target=%.2f actual=%.2f deg RollError=%.3f deg SurfaceNormal=%s\n")
		TEXT("Arm target=%.1f actual=%.1f cm HFOV=%.1f UpOffset=%.1f LookAhead=%.1f\n")
		TEXT("Frame=%s  Reference target: avatar 9-14%%H, horizon top 2-18%%, settled roll <=1 deg"),
		GetCheckpointName(),
		GetPresetName(),
		Movement && Movement->IsMovingOnGround() ? TEXT("Grounded") : TEXT("Falling"),
		*GetNameSafe(Planet),
		CameraRig ? CameraRig->GetCameraDownPitchDegrees() : 0.0f,
		ActualDownPitch,
		RollError,
		*LocalUp.ToCompactString(),
		CameraRig ? CameraRig->GetCameraArmLength() : 0.0f,
		ActualArmLength,
		CameraRig ? CameraRig->GetCameraHorizontalFovDegrees() : 0.0f,
		CameraRig ? CameraRig->GetCameraLocalUpOffset() : 0.0f,
		CameraRig ? CameraRig->GetCameraLookAheadDistance() : 0.0f,
		bFramePass ? TEXT("PASS") : TEXT("WAIT"));

	GEngine->AddOnScreenDebugMessage(
		EverPlanetM2CameraProbe::HudMessageKey,
		0.1f,
		bFramePass ? FColor::Green : FColor::Cyan,
		HudText,
		false,
		FVector2D(0.9f, 0.9f));
}

const TCHAR* AM2GlobeCameraManualTestCharacter::GetCheckpointName(
	const ECameraCheckpoint Checkpoint)
{
	switch (Checkpoint)
	{
	case ECameraCheckpoint::North: return TEXT("North");
	case ECameraCheckpoint::Equator: return TEXT("Equator");
	case ECameraCheckpoint::South: return TEXT("South");
	case ECameraCheckpoint::PoleCross: return TEXT("PoleCross");
	case ECameraCheckpoint::FullLap: return TEXT("FullLap");
	default: return TEXT("Unknown");
	}
}

const TCHAR* AM2GlobeCameraManualTestCharacter::GetCheckpointName() const
{
	return GetCheckpointName(CurrentCheckpoint);
}

const TCHAR* AM2GlobeCameraManualTestCharacter::GetPresetName() const
{
	switch (CurrentPreset)
	{
	case ECameraPreset::Reference: return TEXT("Reference");
	case ECameraPreset::Steeper: return TEXT("Steeper");
	case ECameraPreset::Wide: return TEXT("Wide");
	default: return TEXT("Unknown");
	}
}

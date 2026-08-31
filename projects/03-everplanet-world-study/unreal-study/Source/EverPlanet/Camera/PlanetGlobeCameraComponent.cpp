// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/PlanetGlobeCameraComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

#include "Camera/EverPlanetGlobeCameraCharacter.h"
#include "Camera/GlobeCameraMath.h"
#include "EverPlanet.h"
#include "Planet/PlanetBody.h"
#include "Planet/PlanetGravityComponent.h"
#include "Planet/PlanetGravityMath.h"

namespace EverPlanetGlobeCamera
{
	// Normal surface travel changes local up by only a small amount per frame.
	// A larger discontinuity is treated as a teleport/transfer and is reseeded
	// instead of interpolating a camera arc through the planet.
	constexpr double DiscontinuousSurfaceNormalDotThreshold = 0.5;
}

UPlanetGlobeCameraComponent::UPlanetGlobeCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UPlanetGlobeCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!BindOwnerComponents())
	{
		SetComponentTickEnabled(false);
		return;
	}

	AddTickPrerequisiteComponent(CharacterMovement);
	CameraBoomComponent->PrimaryComponentTick.TickGroup = TG_PostPhysics;
	CameraBoomComponent->AddTickPrerequisiteComponent(this);
	ConfigureCameraComponents();
	ClearCameraFrameState();
	UpdateGlobeCamera(0.0f, true);
}

void UPlanetGlobeCameraComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (CharacterMovement)
	{
		RemoveTickPrerequisiteComponent(CharacterMovement);
	}
	if (CameraBoomComponent)
	{
		CameraBoomComponent->RemoveTickPrerequisiteComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UPlanetGlobeCameraComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateGlobeCamera(DeltaTime, false);
}

void UPlanetGlobeCameraComponent::AddLookInput(
	const float Yaw,
	const float Pitch)
{
	if (!FMath::IsFinite(Yaw) || !FMath::IsFinite(Pitch))
	{
		return;
	}

	PendingYawInput += Yaw;
	PendingPitchInput += Pitch;
}

bool UPlanetGlobeCameraComponent::ApplyCameraTuning(
	const float InArmLength,
	const float InDownPitchDegrees,
	const float InHorizontalFovDegrees,
	const float InLocalUpOffset,
	const float InLookAheadDistance,
	const bool bSnapCameraRotation)
{
	if (!FMath::IsFinite(InArmLength)
		|| !FMath::IsFinite(InDownPitchDegrees)
		|| !FMath::IsFinite(InHorizontalFovDegrees)
		|| !FMath::IsFinite(InLocalUpOffset)
		|| !FMath::IsFinite(InLookAheadDistance))
	{
		return false;
	}

	CameraArmLength = FMath::Max(InArmLength, 100.0f);
	CameraDownPitchDegrees = FMath::Clamp(
		InDownPitchDegrees,
		MinimumCameraDownPitchDegrees,
		MaximumCameraDownPitchDegrees);
	CameraHorizontalFovDegrees = FMath::Clamp(
		InHorizontalFovDegrees,
		5.0f,
		170.0f);
	CameraLocalUpOffset = InLocalUpOffset;
	CameraLookAheadDistance = InLookAheadDistance;
	if (CameraBoomComponent && FollowCameraComponent)
	{
		ConfigureCameraComponents();
	}
	return bSnapCameraRotation
		? UpdateGlobeCamera(0.0f, true)
		: true;
}

bool UPlanetGlobeCameraComponent::ResetCameraFrame(
	const FVector& PreferredTangentForward,
	const bool bSnapCameraRotation)
{
	PendingReseedForward = PreferredTangentForward;
	bPendingCameraReseed = true;
	bSnapPendingReseed = bSnapCameraRotation;
	return bSnapCameraRotation
		? UpdateGlobeCamera(0.0f, true)
		: true;
}

bool UPlanetGlobeCameraComponent::BindOwnerComponents()
{
	CharacterOwner = Cast<AEverPlanetGlobeCameraCharacter>(GetOwner());
	if (!CharacterOwner)
	{
		UE_LOG(
			LogEverPlanet,
			Error,
			TEXT("PlanetGlobeCameraComponent on '%s' requires an AEverPlanetGlobeCameraCharacter owner."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	CharacterMovement = CharacterOwner->GetCharacterMovement();
	PlanetGravityComponent = CharacterOwner->GetPlanetGravityComponent();
	CameraBoomComponent = CharacterOwner->GetCameraBoom();
	FollowCameraComponent = CharacterOwner->GetFollowCamera();
	if (!CharacterMovement
		|| !PlanetGravityComponent
		|| !CameraBoomComponent
		|| !FollowCameraComponent)
	{
		UE_LOG(
			LogEverPlanet,
			Error,
			TEXT("PlanetGlobeCameraComponent on '%s' could not bind movement, gravity, boom, and camera components."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	return true;
}

void UPlanetGlobeCameraComponent::ConfigureCameraComponents()
{
	MinimumCameraDownPitchDegrees = FMath::Clamp(
		MinimumCameraDownPitchDegrees,
		0.0f,
		89.0f);
	MaximumCameraDownPitchDegrees = FMath::Clamp(
		MaximumCameraDownPitchDegrees,
		MinimumCameraDownPitchDegrees,
		89.0f);
	CameraDownPitchDegrees = FMath::Clamp(
		CameraDownPitchDegrees,
		MinimumCameraDownPitchDegrees,
		MaximumCameraDownPitchDegrees);

	CameraArmLength = FMath::Max(CameraArmLength, 100.0f);
	CameraHorizontalFovDegrees = FMath::Clamp(
		CameraHorizontalFovDegrees,
		5.0f,
		170.0f);
	CameraCollisionProbeSize = FMath::Max(CameraCollisionProbeSize, 1.0f);
	CameraTeleportDistanceThreshold = FMath::Max(
		CameraTeleportDistanceThreshold,
		100.0f);

	CameraBoomComponent->TargetArmLength = CameraArmLength;
	CameraBoomComponent->bUsePawnControlRotation = false;
	CameraBoomComponent->bInheritPitch = false;
	CameraBoomComponent->bInheritYaw = false;
	CameraBoomComponent->bInheritRoll = false;
	CameraBoomComponent->SetUsingAbsoluteRotation(true);
	CameraBoomComponent->bEnableCameraLag = false;
	CameraBoomComponent->bEnableCameraRotationLag = false;
	CameraBoomComponent->bDoCollisionTest = true;
	CameraBoomComponent->ProbeSize = CameraCollisionProbeSize;
	CameraBoomComponent->ProbeChannel = ECC_Camera;

	FollowCameraComponent->bUsePawnControlRotation = false;
	FollowCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
	FollowCameraComponent->SetFieldOfView(CameraHorizontalFovDegrees);
}

bool UPlanetGlobeCameraComponent::UpdateGlobeCamera(
	const float DeltaSeconds,
	const bool bSnapCameraRotation)
{
	if (!CharacterOwner
		|| !PlanetGravityComponent
		|| !CameraBoomComponent)
	{
		return false;
	}

	APlanetBody* CurrentPlanet = PlanetGravityComponent->GetGravitySource();
	FVector LocalUp = FVector::ZeroVector;
	if (!IsValid(CurrentPlanet)
		|| !PlanetGravityComponent->TryGetSurfaceNormal(LocalUp))
	{
		ClearCameraFrameState();
		return false;
	}

	const bool bPlanetChanged = CameraPlanet != CurrentPlanet;
	const FVector CurrentOwnerLocation = CharacterOwner->GetActorLocation();
	const bool bLocationJump = bHasPreviousOwnerLocation
		&& FVector::DistSquared(PreviousOwnerLocation, CurrentOwnerLocation)
			> FMath::Square(static_cast<double>(CameraTeleportDistanceThreshold));
	if (bPlanetChanged)
	{
		CameraPlanet = CurrentPlanet;
		PreviousSurfaceNormal = FVector::ZeroVector;
		CameraTangentForward = FVector::ZeroVector;
		bHasCameraRotation = false;
	}

	const bool bHasPreviousSurfaceFrame =
		!PreviousSurfaceNormal.IsNearlyZero()
		&& !CameraTangentForward.IsNearlyZero();
	const bool bDiscontinuousSurfaceTransfer = bLocationJump
		|| (bHasPreviousSurfaceFrame
			&& FVector::DotProduct(PreviousSurfaceNormal, LocalUp)
				< EverPlanetGlobeCamera::DiscontinuousSurfaceNormalDotThreshold);

	FVector NextTangentForward = FVector::ZeroVector;
	if (bPendingCameraReseed)
	{
		NextTangentForward = FPlanetGravityMath::ProjectOntoTangentPlane(
			PendingReseedForward,
			LocalUp).GetSafeNormal();
	}
	else if (bHasPreviousSurfaceFrame && !bDiscontinuousSurfaceTransfer)
	{
		FGlobeCameraMath::TryParallelTransportTangent(
			PreviousSurfaceNormal,
			LocalUp,
			CameraTangentForward,
			NextTangentForward);
	}

	if (NextTangentForward.IsNearlyZero()
		&& !TrySeedCameraHeading(
			LocalUp,
			bDiscontinuousSurfaceTransfer,
			NextTangentForward))
	{
		ClearCameraFrameState();
		return false;
	}

	if (!FMath::IsNearlyZero(PendingYawInput))
	{
		const double YawDegrees = static_cast<double>(PendingYawInput)
			* static_cast<double>(CameraYawDegreesPerInputUnit);
		NextTangentForward = FQuat(
			LocalUp,
			FMath::DegreesToRadians(YawDegrees))
			.RotateVector(NextTangentForward).GetSafeNormal();
	}
	CameraDownPitchDegrees = FMath::Clamp(
		CameraDownPitchDegrees
			- PendingPitchInput * CameraPitchDegreesPerInputUnit,
		MinimumCameraDownPitchDegrees,
		MaximumCameraDownPitchDegrees);
	PendingYawInput = 0.0f;
	PendingPitchInput = 0.0f;

	FGlobeCameraFrame TargetFrame;
	if (!FGlobeCameraMath::TryBuildViewFrame(
			LocalUp,
			NextTangentForward,
			CameraDownPitchDegrees,
			TargetFrame))
	{
		ClearCameraFrameState();
		return false;
	}

	const bool bSnap = bSnapCameraRotation
		|| bPlanetChanged
		|| bDiscontinuousSurfaceTransfer
		|| (bPendingCameraReseed && bSnapPendingReseed)
		|| !bHasCameraRotation
		|| (DeltaSeconds <= 0.0f && bSnapCameraRotation)
		|| CameraRotationInterpolationSpeed <= 0.0f;
	if (bSnap)
	{
		CurrentCameraRotation = TargetFrame.ViewRotation;
	}
	else
	{
		const double Alpha = 1.0 - FMath::Exp(
			-static_cast<double>(CameraRotationInterpolationSpeed)
			* static_cast<double>(DeltaSeconds));
		CurrentCameraRotation = FQuat::Slerp(
			CurrentCameraRotation,
			TargetFrame.ViewRotation,
			FMath::Clamp(Alpha, 0.0, 1.0)).GetNormalized();
	}

	CameraTangentForward = TargetFrame.TangentForward;
	PreviousSurfaceNormal = TargetFrame.LocalUp;
	PreviousOwnerLocation = CurrentOwnerLocation;
	bHasCameraRotation = true;
	bHasPreviousOwnerLocation = true;
	bPendingCameraReseed = false;
	bSnapPendingReseed = true;
	PendingReseedForward = FVector::ZeroVector;

	FVector SmoothedTangentForward = FVector::VectorPlaneProject(
		CurrentCameraRotation.GetAxisX(),
		TargetFrame.LocalUp).GetSafeNormal();
	if (SmoothedTangentForward.IsNearlyZero())
	{
		SmoothedTangentForward = TargetFrame.TangentForward;
	}
	CameraBoomComponent->TargetOffset =
		TargetFrame.LocalUp * CameraLocalUpOffset
		+ SmoothedTangentForward * CameraLookAheadDistance;
	CameraBoomComponent->SetWorldRotation(CurrentCameraRotation);
	return true;
}

bool UPlanetGlobeCameraComponent::TrySeedCameraHeading(
	const FVector& LocalUp,
	const bool bPreferActorForward,
	FVector& OutTangentForward) const
{
	OutTangentForward = FVector::ZeroVector;
	const FVector PreferredForward = !bPreferActorForward
		&& FollowCameraComponent
		&& bHasCameraRotation
		? FollowCameraComponent->GetForwardVector()
		: CharacterOwner->GetActorForwardVector();
	FVector TangentRight = FVector::ZeroVector;
	return FPlanetGravityMath::TryBuildTangentBasis(
		PreferredForward,
		LocalUp,
		OutTangentForward,
		TangentRight);
}

void UPlanetGlobeCameraComponent::ClearCameraFrameState()
{
	CameraPlanet = nullptr;
	PreviousSurfaceNormal = FVector::ZeroVector;
	CameraTangentForward = FVector::ZeroVector;
	CurrentCameraRotation = FQuat::Identity;
	bHasCameraRotation = false;
	PreviousOwnerLocation = FVector::ZeroVector;
	bHasPreviousOwnerLocation = false;
	PendingYawInput = 0.0f;
	PendingPitchInput = 0.0f;
}

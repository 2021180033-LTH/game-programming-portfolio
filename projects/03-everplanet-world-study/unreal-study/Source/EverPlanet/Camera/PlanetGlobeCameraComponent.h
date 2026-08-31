// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlanetGlobeCameraComponent.generated.h"

class AEverPlanetGlobeCameraCharacter;
class APlanetBody;
class UCameraComponent;
class UCharacterMovementComponent;
class UPlanetGravityComponent;
class USpringArmComponent;

/**
 * Post-physics camera rig that aligns the inherited Spring Arm to planet local up.
 * It owns camera composition and smoothing without changing Character tick timing.
 */
UCLASS(ClassGroup = (EverPlanet), meta = (BlueprintSpawnableComponent))
class EVERPLANET_API UPlanetGlobeCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlanetGlobeCameraComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void AddLookInput(float Yaw, float Pitch);

	/** Applies a complete composition preset and optionally snaps to it immediately. */
	UFUNCTION(BlueprintCallable, Category = "Globe Camera")
	bool ApplyCameraTuning(
		float InArmLength,
		float InDownPitchDegrees,
		float InHorizontalFovDegrees,
		float InLocalUpOffset,
		float InLookAheadDistance,
		bool bSnapCameraRotation = true);

	/** Reseeds the tangent heading; snaps on discontinuous transfers by default. */
	bool ResetCameraFrame(
		const FVector& PreferredTangentForward,
		bool bSnapCameraRotation = true);

	UFUNCTION(BlueprintPure, Category = "Globe Camera")
	FVector GetCameraTangentForward() const { return CameraTangentForward; }

	UFUNCTION(BlueprintPure, Category = "Globe Camera")
	FVector GetCameraSurfaceNormal() const { return PreviousSurfaceNormal; }

	UFUNCTION(BlueprintPure, Category = "Globe Camera")
	float GetCameraDownPitchDegrees() const { return CameraDownPitchDegrees; }

	UFUNCTION(BlueprintPure, Category = "Globe Camera")
	float GetCameraArmLength() const { return CameraArmLength; }

	UFUNCTION(BlueprintPure, Category = "Globe Camera")
	float GetCameraHorizontalFovDegrees() const { return CameraHorizontalFovDegrees; }

	UFUNCTION(BlueprintPure, Category = "Globe Camera")
	float GetCameraLocalUpOffset() const { return CameraLocalUpOffset; }

	UFUNCTION(BlueprintPure, Category = "Globe Camera")
	float GetCameraLookAheadDistance() const { return CameraLookAheadDistance; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Slant distance from the local-up/look-ahead pivot to the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Globe Camera|Composition", meta = (ClampMin = "100.0", UIMin = "100.0", Units = "cm"))
	float CameraArmLength = 1600.0f;

	/** Downward angle measured from the local tangent plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Globe Camera|Composition", meta = (ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "89.0", Units = "deg"))
	float CameraDownPitchDegrees = 38.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Globe Camera|Composition", meta = (ClampMin = "5.0", ClampMax = "170.0", Units = "deg"))
	float CameraHorizontalFovDegrees = 60.0f;

	/** World-space pivot offset recalculated along current planet local up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Globe Camera|Composition", meta = (Units = "cm"))
	float CameraLocalUpOffset = 100.0f;

	/** World-space pivot offset toward the stable tangent heading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Globe Camera|Composition", meta = (Units = "cm"))
	float CameraLookAheadDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Globe Camera|Smoothing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CameraRotationInterpolationSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Globe Camera|Input", meta = (ClampMin = "0.0", Units = "deg"))
	float CameraYawDegreesPerInputUnit = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Globe Camera|Input", meta = (ClampMin = "0.0", Units = "deg"))
	float CameraPitchDegreesPerInputUnit = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Globe Camera|Input", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float MinimumCameraDownPitchDegrees = 32.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Globe Camera|Input", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float MaximumCameraDownPitchDegrees = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Globe Camera|Collision", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float CameraCollisionProbeSize = 18.0f;

	/** A same-planet location jump above this distance is treated as a teleport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Globe Camera|Continuity", meta = (ClampMin = "100.0", UIMin = "100.0", Units = "cm"))
	float CameraTeleportDistanceThreshold = 1000.0f;

private:
	bool BindOwnerComponents();
	void ConfigureCameraComponents();
	bool UpdateGlobeCamera(float DeltaSeconds, bool bSnapCameraRotation);
	bool TrySeedCameraHeading(
		const FVector& LocalUp,
		bool bPreferActorForward,
		FVector& OutTangentForward) const;
	void ClearCameraFrameState();

	UPROPERTY(Transient)
	TObjectPtr<AEverPlanetGlobeCameraCharacter> CharacterOwner;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> CharacterMovement;

	UPROPERTY(Transient)
	TObjectPtr<UPlanetGravityComponent> PlanetGravityComponent;

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CameraBoomComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> FollowCameraComponent;

	UPROPERTY(Transient)
	TObjectPtr<APlanetBody> CameraPlanet;

	UPROPERTY(Transient)
	FVector PreviousSurfaceNormal = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector CameraTangentForward = FVector::ZeroVector;

	UPROPERTY(Transient)
	FQuat CurrentCameraRotation = FQuat::Identity;

	UPROPERTY(Transient)
	FVector PreviousOwnerLocation = FVector::ZeroVector;

	FVector PendingReseedForward = FVector::ZeroVector;
	float PendingYawInput = 0.0f;
	float PendingPitchInput = 0.0f;
	bool bHasCameraRotation = false;
	bool bHasPreviousOwnerLocation = false;
	bool bPendingCameraReseed = false;
	bool bSnapPendingReseed = true;
};

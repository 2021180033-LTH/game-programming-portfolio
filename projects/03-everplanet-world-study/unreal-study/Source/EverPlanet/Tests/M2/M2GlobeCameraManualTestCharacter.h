// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/EverPlanetGlobeCameraCharacter.h"
#include "M2GlobeCameraManualTestCharacter.generated.h"

/** Test-only pawn for repeatable M2 camera framing and pole-continuity checks. */
UCLASS(Blueprintable)
class EVERPLANET_API AM2GlobeCameraManualTestCharacter
	: public AEverPlanetGlobeCameraCharacter
{
	GENERATED_BODY()

public:
	AM2GlobeCameraManualTestCharacter();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	enum class ECameraCheckpoint : uint8
	{
		North,
		Equator,
		South,
		PoleCross,
		FullLap
	};

	enum class ECameraPreset : uint8
	{
		Reference,
		Steeper,
		Wide
	};

	void SelectNorthCheckpoint();
	void SelectEquatorCheckpoint();
	void SelectSouthCheckpoint();
	void SelectPoleCrossCheckpoint();
	void SelectFullLapCheckpoint();
	void RestartCurrentCheckpoint();
	void SelectReferencePreset();
	void SelectSteeperPreset();
	void SelectWidePreset();
	void ToggleCameraHud();

	void ActivateCheckpoint(ECameraCheckpoint NewCheckpoint);
	void ApplyPreset(ECameraPreset NewPreset);
	bool TryGetCheckpointFrame(
		ECameraCheckpoint Checkpoint,
		FVector& OutSurfaceNormal,
		FVector& OutTangentForward) const;
	void DrawCameraHud() const;
	static const TCHAR* GetCheckpointName(ECameraCheckpoint Checkpoint);
	const TCHAR* GetCheckpointName() const;
	const TCHAR* GetPresetName() const;

	UPROPERTY(EditAnywhere, Category = "M2 Camera Probe")
	bool bShowCameraHud = true;

	bool bManualCameraProbeEnabled = false;
	ECameraCheckpoint CurrentCheckpoint = ECameraCheckpoint::North;
	ECameraPreset CurrentPreset = ECameraPreset::Reference;
};

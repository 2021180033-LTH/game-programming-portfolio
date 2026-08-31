// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EverPlanetRegionPortal.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

/** Graybox-visible portal that asks the active catalog to execute one TravelLinkId. */
UCLASS()
class EVERPLANET_API AEverPlanetRegionPortal : public AActor
{
	GENERATED_BODY()

public:
	AEverPlanetRegionPortal();

	UFUNCTION(BlueprintPure, Category = "EverPlanet|World")
	FName GetTravelLinkId() const { return TravelLinkId; }

	UFUNCTION(BlueprintPure, Category = "EverPlanet|World")
	FName GetPortalId() const { return PortalId; }

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PortalVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "EverPlanet|World", meta = (AllowPrivateAccess = "true"))
	FName TravelLinkId = NAME_None;

	/** Stable local identity checked against the catalog link's SourcePortalId. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "EverPlanet|World", meta = (AllowPrivateAccess = "true"))
	FName PortalId = NAME_None;

	UPROPERTY(Transient)
	bool bTravelRequestSubmitted = false;
};

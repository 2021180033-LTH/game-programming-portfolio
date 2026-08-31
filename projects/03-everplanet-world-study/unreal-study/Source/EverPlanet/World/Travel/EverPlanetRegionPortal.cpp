// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/Travel/EverPlanetRegionPortal.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

#include "EverPlanet.h"
#include "World/Progression/EverPlanetWorldProgressSubsystem.h"

AEverPlanetRegionPortal::AEverPlanetRegionPortal()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PortalVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalVisual"));
	PortalVisual->SetupAttachment(SceneRoot);
	PortalVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PortalVisual->SetRelativeLocation(FVector(0.0, 0.0, 200.0));
	PortalVisual->SetRelativeScale3D(FVector(0.35, 2.5, 4.0));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PortalVisual->SetStaticMesh(CubeMesh.Object);
	}

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(SceneRoot);
	Trigger->SetRelativeLocation(FVector(0.0, 0.0, 200.0));
	Trigger->SetBoxExtent(FVector(100.0, 300.0, 400.0));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->OnComponentBeginOverlap.AddDynamic(
		this,
		&AEverPlanetRegionPortal::OnTriggerBeginOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(
		this,
		&AEverPlanetRegionPortal::OnTriggerEndOverlap);
}

void AEverPlanetRegionPortal::OnTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bTravelRequestSubmitted)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UEverPlanetWorldProgressSubsystem* ProgressSubsystem = GameInstance
		? GameInstance->GetSubsystem<UEverPlanetWorldProgressSubsystem>()
		: nullptr;
	if (!ProgressSubsystem)
	{
		UE_LOG(
			LogEverPlanet,
			Error,
			TEXT("WORLD_REGION_PORTAL_REJECTED=link=%s reason=no-subsystem"),
			*TravelLinkId.ToString());
		return;
	}

	if (!ProgressSubsystem->RequestTravelByLinkId(TravelLinkId, PortalId))
	{
		UE_LOG(
			LogEverPlanet,
			Warning,
			TEXT("WORLD_REGION_PORTAL_REJECTED=link=%s portal=%s reason=%s"),
			*TravelLinkId.ToString(),
			*PortalId.ToString(),
			*ProgressSubsystem->GetLastFailureMessage());
		return;
	}

	bTravelRequestSubmitted = true;
	UE_LOG(
		LogEverPlanet,
		Log,
		TEXT("WORLD_REGION_PORTAL_ACCEPTED=link=%s portal=%s pawn=%s"),
		*TravelLinkId.ToString(),
		*PortalId.ToString(),
		*GetNameSafe(Pawn));
}

void AEverPlanetRegionPortal::OnTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		// If travel failed and the source world stayed alive, leaving/re-entering the
		// portal becomes an explicit retry instead of a permanent actor-local lock.
		bTravelRequestSubmitted = false;
	}
}

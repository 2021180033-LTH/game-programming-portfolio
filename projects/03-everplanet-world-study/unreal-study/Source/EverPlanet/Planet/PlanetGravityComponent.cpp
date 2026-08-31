// Copyright Epic Games, Inc. All Rights Reserved.

#include "Planet/PlanetGravityComponent.h"

#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "EverPlanet.h"
#include "Planet/PlanetBody.h"

UPlanetGravityComponent::UPlanetGravityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UPlanetGravityComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner = Cast<ACharacter>(GetOwner());
	if (!CharacterOwner)
	{
		UE_LOG(
			LogEverPlanet,
			Error,
			TEXT("PlanetGravityComponent on '%s' requires an ACharacter owner."),
			*GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}

	CharacterMovement = CharacterOwner->GetCharacterMovement();
	if (!CharacterMovement)
	{
		UE_LOG(
			LogEverPlanet,
			Error,
			TEXT("PlanetGravityComponent on '%s' could not find a CharacterMovementComponent."),
			*GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}

	// Gravity must be current before Character Movement performs its PrePhysics tick.
	CharacterMovement->AddTickPrerequisiteComponent(this);
	RefreshGravitySource();
}

void UPlanetGravityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreDefaultGravityDirection();

	if (CharacterMovement)
	{
		CharacterMovement->RemoveTickPrerequisiteComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UPlanetGravityComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool bRefreshedSource = false;
	if (bAutoSelectNearestPlanet && !IsValid(GravitySource))
	{
		TimeSincePlanetSearch += DeltaTime;
		const bool bSourceWasLost = bAppliedPlanetGravity;
		if (bSourceWasLost
			|| PlanetSearchInterval <= 0.0f
			|| TimeSincePlanetSearch >= PlanetSearchInterval)
		{
			RefreshGravitySource();
			bRefreshedSource = true;
		}
	}

	if (!bRefreshedSource)
	{
		ApplyGravityDirection();
	}
}

void UPlanetGravityComponent::SetGravitySource(APlanetBody* NewGravitySource)
{
	APlanetBody* PreviousSource = GravitySource;
	bAutoSelectNearestPlanet = false;
	GravitySource = NewGravitySource;
	if (PreviousSource != GravitySource && CharacterOwner)
	{
		CharacterOwner->ConsumeMovementInputVector();
	}
	ApplyGravityDirection();
}

void UPlanetGravityComponent::RefreshGravitySource()
{
	TimeSincePlanetSearch = 0.0f;

	APlanetBody* PreviousSource = GravitySource;
	if (bAutoSelectNearestPlanet)
	{
		GravitySource = FindNearestPlanet();
	}
	else if (!IsValid(GravitySource))
	{
		GravitySource = nullptr;
	}

	if (PreviousSource != GravitySource)
	{
		// Input may have been accumulated against the previous gravity frame before
		// this PrePhysics refresh. Dropping that single frame prevents it from being
		// interpreted in the newly acquired planet frame.
		if (CharacterOwner)
		{
			CharacterOwner->ConsumeMovementInputVector();
		}

		UE_LOG(
			LogEverPlanet,
			Display,
			TEXT("Planet gravity source for '%s' changed from '%s' to '%s'."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(PreviousSource),
			*GetNameSafe(GravitySource));
	}

	ApplyGravityDirection();
}

bool UPlanetGravityComponent::TryGetSurfaceNormal(FVector& OutSurfaceNormal) const
{
	OutSurfaceNormal = FVector::ZeroVector;
	return CharacterOwner
		&& IsValid(GravitySource)
		&& GravitySource->TryGetSurfaceNormal(
			CharacterOwner->GetActorLocation(),
			OutSurfaceNormal);
}

bool UPlanetGravityComponent::TryGetGravityDirection(FVector& OutGravityDirection) const
{
	OutGravityDirection = FVector::ZeroVector;
	return CharacterOwner
		&& IsValid(GravitySource)
		&& GravitySource->TryGetGravityDirection(
			CharacterOwner->GetActorLocation(),
			OutGravityDirection);
}

APlanetBody* UPlanetGravityComponent::FindNearestPlanet() const
{
	if (!GetWorld() || !CharacterOwner)
	{
		return nullptr;
	}

	APlanetBody* NearestPlanet = nullptr;
	double NearestSurfaceDistance = TNumericLimits<double>::Max();
	const FVector OwnerLocation = CharacterOwner->GetActorLocation();

	for (TActorIterator<APlanetBody> PlanetIterator(GetWorld()); PlanetIterator; ++PlanetIterator)
	{
		APlanetBody* Candidate = *PlanetIterator;
		if (!IsValid(Candidate))
		{
			continue;
		}

		const double CenterDistance = FVector::Distance(
			OwnerLocation,
			Candidate->GetPlanetCenter());
		const double SurfaceDistance = FMath::Abs(
			CenterDistance - static_cast<double>(Candidate->GetSurfaceRadius()));
		if (SurfaceDistance < NearestSurfaceDistance)
		{
			NearestSurfaceDistance = SurfaceDistance;
			NearestPlanet = Candidate;
		}
	}

	return NearestPlanet;
}

void UPlanetGravityComponent::ApplyGravityDirection()
{
	if (!CharacterMovement)
	{
		return;
	}

	FVector GravityDirection;
	if (TryGetGravityDirection(GravityDirection))
	{
		CharacterMovement->SetGravityDirection(GravityDirection);
		bAppliedPlanetGravity = true;
		return;
	}

	RestoreDefaultGravityDirection();
}

void UPlanetGravityComponent::RestoreDefaultGravityDirection()
{
	if (!CharacterMovement || !bAppliedPlanetGravity)
	{
		return;
	}

	CharacterMovement->SetGravityDirection(
		UCharacterMovementComponent::DefaultGravityDirection);
	bAppliedPlanetGravity = false;
}

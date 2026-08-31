// Copyright Epic Games, Inc. All Rights Reserved.

#include "Planet/PlanetBody.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

#include "EverPlanet.h"
#include "Planet/PlanetGravityMath.h"

namespace PlanetBodyConstants
{
	constexpr float EngineSphereRadius = 50.0f;
	constexpr float MinimumSurfaceRadius = 100.0f;
}

APlanetBody::APlanetBody()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	SetRootComponent(SceneRoot);

	PlanetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlanetMesh"));
	PlanetMesh->SetupAttachment(SceneRoot);
	PlanetMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	PlanetMesh->SetGenerateOverlapEvents(false);
	PlanetMesh->SetMobility(EComponentMobility::Static);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		PlanetMesh->SetStaticMesh(SphereMesh.Object);
	}
}

void APlanetBody::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyAuthoritativeScale();
}

void APlanetBody::BeginPlay()
{
	Super::BeginPlay();
	ApplyAuthoritativeScale();
}

void APlanetBody::ApplyAuthoritativeScale()
{
	if (!GetActorScale3D().Equals(FVector::OneVector))
	{
		UE_LOG(
			LogEverPlanet,
			Warning,
			TEXT("PlanetBody '%s' actor scale was reset to 1. Use SurfaceRadius to resize a spherical planet."),
			*GetNameSafe(this));
		SetActorScale3D(FVector::OneVector);
	}

	SurfaceRadius = FMath::Max(SurfaceRadius, PlanetBodyConstants::MinimumSurfaceRadius);
	const float UniformMeshScale = SurfaceRadius / PlanetBodyConstants::EngineSphereRadius;
	PlanetMesh->SetRelativeScale3D(FVector(UniformMeshScale));
}

FVector APlanetBody::GetPlanetCenter() const
{
	return GetActorLocation();
}

bool APlanetBody::TryGetSurfaceNormal(
	const FVector& WorldLocation,
	FVector& OutSurfaceNormal) const
{
	return FPlanetGravityMath::TryGetSurfaceNormal(
		WorldLocation,
		GetPlanetCenter(),
		OutSurfaceNormal);
}

bool APlanetBody::TryGetGravityDirection(
	const FVector& WorldLocation,
	FVector& OutGravityDirection) const
{
	return FPlanetGravityMath::TryGetGravityDirection(
		WorldLocation,
		GetPlanetCenter(),
		OutGravityDirection);
}

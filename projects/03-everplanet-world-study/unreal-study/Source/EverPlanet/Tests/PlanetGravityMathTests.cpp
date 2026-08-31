// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#include "Planet/PlanetGravityMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlanetGravityMathTest,
	"EverPlanet.Planet.GravityMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlanetGravityMathTest::RunTest(const FString& Parameters)
{
	struct FDirectionCase
	{
		FVector WorldLocation;
		FVector ExpectedGravityDirection;
	};

	const FVector PlanetCenter(125.0, -250.0, 500.0);
	const TArray<FDirectionCase> DirectionCases = {
		{PlanetCenter + FVector(3000.0, 0.0, 0.0), FVector(-1.0, 0.0, 0.0)},
		{PlanetCenter + FVector(-3000.0, 0.0, 0.0), FVector(1.0, 0.0, 0.0)},
		{PlanetCenter + FVector(0.0, 3000.0, 0.0), FVector(0.0, -1.0, 0.0)},
		{PlanetCenter + FVector(0.0, -3000.0, 0.0), FVector(0.0, 1.0, 0.0)},
		{PlanetCenter + FVector(0.0, 0.0, 3000.0), FVector(0.0, 0.0, -1.0)},
		{PlanetCenter + FVector(0.0, 0.0, -3000.0), FVector(0.0, 0.0, 1.0)},
		{PlanetCenter + FVector(1000.0, 2000.0, 2000.0), FVector(-1.0 / 3.0, -2.0 / 3.0, -2.0 / 3.0)},
	};

	for (int32 CaseIndex = 0; CaseIndex < DirectionCases.Num(); ++CaseIndex)
	{
		FVector GravityDirection;
		const bool bHasDirection = FPlanetGravityMath::TryGetGravityDirection(
			DirectionCases[CaseIndex].WorldLocation,
			PlanetCenter,
			GravityDirection);

		TestTrue(
			FString::Printf(TEXT("Direction case %d produces a direction"), CaseIndex),
			bHasDirection);
		TestTrue(
			FString::Printf(TEXT("Direction case %d points toward the center"), CaseIndex),
			GravityDirection.Equals(
				DirectionCases[CaseIndex].ExpectedGravityDirection,
				UE_KINDA_SMALL_NUMBER));
	}

	FVector InvalidDirection(1.0, 1.0, 1.0);
	TestFalse(
		TEXT("A location at the exact center has no gravity direction"),
		FPlanetGravityMath::TryGetGravityDirection(
			PlanetCenter,
			PlanetCenter,
			InvalidDirection));
	TestTrue(
		TEXT("An invalid gravity direction is reset instead of becoming NaN"),
		InvalidDirection.IsZero());

	const double QuietNaN = std::numeric_limits<double>::quiet_NaN();
	FVector NonFiniteDirection(1.0, 1.0, 1.0);
	TestFalse(
		TEXT("A non-finite location is rejected"),
		FPlanetGravityMath::TryGetGravityDirection(
			FVector(QuietNaN, 0.0, 0.0),
			PlanetCenter,
			NonFiniteDirection));
	TestTrue(
		TEXT("A rejected non-finite direction is reset"),
		NonFiniteDirection.IsZero());

	FVector InfiniteCenterNormal(1.0, 1.0, 1.0);
	TestFalse(
		TEXT("A non-finite planet center is rejected"),
		FPlanetGravityMath::TryGetSurfaceNormal(
			FVector::ZeroVector,
			FVector(std::numeric_limits<double>::infinity(), 0.0, 0.0),
			InfiniteCenterNormal));
	TestTrue(
		TEXT("A rejected non-finite surface normal is reset"),
		InfiniteCenterNormal.IsZero());

	FVector NearCenterDirection(1.0, 1.0, 1.0);
	TestFalse(
		TEXT("A numerically degenerate near-center direction is rejected"),
		FPlanetGravityMath::TryGetGravityDirection(
			PlanetCenter + FVector(1.0e-12, 0.0, 0.0),
			PlanetCenter,
			NearCenterDirection));
	TestTrue(
		TEXT("A rejected near-center direction is reset"),
		NearCenterDirection.IsZero());

	FVector SurfaceNormal;
	TestTrue(
		TEXT("Surface normal is available away from the center"),
		FPlanetGravityMath::TryGetSurfaceNormal(
			PlanetCenter + FVector(0.0, 0.0, 3000.0),
			PlanetCenter,
			SurfaceNormal));
	TestTrue(
		TEXT("Surface normal is opposite to gravity"),
		SurfaceNormal.Equals(FVector::UpVector, UE_KINDA_SMALL_NUMBER));

	const FVector TangentProjection = FPlanetGravityMath::ProjectOntoTangentPlane(
		FVector(2.0, 3.0, 4.0),
		FVector::UpVector);
	TestTrue(
		TEXT("Tangent projection removes the local vertical component"),
		FMath::IsNearlyZero(
			FVector::DotProduct(TangentProjection, FVector::UpVector),
			UE_KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Tangent projection preserves horizontal components"),
		TangentProjection.Equals(FVector(2.0, 3.0, 0.0), UE_KINDA_SMALL_NUMBER));

	FVector TangentForward;
	FVector TangentRight;
	TestTrue(
		TEXT("A tangent basis is created when preferred forward is parallel to up"),
		FPlanetGravityMath::TryBuildTangentBasis(
			FVector::UpVector,
			FVector::UpVector,
			TangentForward,
			TangentRight));
	TestTrue(TEXT("Tangent forward is normalized"), TangentForward.IsNormalized());
	TestTrue(TEXT("Tangent right is normalized"), TangentRight.IsNormalized());
	TestTrue(
		TEXT("Tangent forward is perpendicular to up"),
		FMath::IsNearlyZero(
			FVector::DotProduct(TangentForward, FVector::UpVector),
			UE_KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Tangent right is perpendicular to up"),
		FMath::IsNearlyZero(
			FVector::DotProduct(TangentRight, FVector::UpVector),
			UE_KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Forward cross right reconstructs local up"),
		FVector::CrossProduct(TangentForward, TangentRight)
			.Equals(FVector::UpVector, UE_KINDA_SMALL_NUMBER));

	FVector InvalidForward;
	FVector InvalidRight;
	TestFalse(
		TEXT("A zero surface normal cannot produce a tangent basis"),
		FPlanetGravityMath::TryBuildTangentBasis(
			FVector::ForwardVector,
			FVector::ZeroVector,
			InvalidForward,
			InvalidRight));

	const FVector InitialForward = FVector::ForwardVector;
	FVector PreviousForward = InitialForward;
	for (int32 Step = 0; Step <= 360; ++Step)
	{
		const double AngleRadians = FMath::DegreesToRadians(static_cast<double>(Step));
		const FVector GreatCircleUp(
			FMath::Sin(AngleRadians),
			0.0,
			FMath::Cos(AngleRadians));
		const FVector ExpectedGreatCircleForward(
			FMath::Cos(AngleRadians),
			0.0,
			-FMath::Sin(AngleRadians));

		FVector GreatCircleForward;
		FVector GreatCircleRight;
		const bool bBuiltFrame = FPlanetGravityMath::TryBuildTangentBasis(
			PreviousForward,
			GreatCircleUp,
			GreatCircleForward,
			GreatCircleRight);
		TestTrue(
			FString::Printf(TEXT("Great-circle frame %d is valid"), Step),
			bBuiltFrame);

		if (!bBuiltFrame)
		{
			break;
		}

		TestTrue(
			FString::Printf(TEXT("Great-circle frame %d does not flip"), Step),
			FVector::DotProduct(PreviousForward, GreatCircleForward) > 0.999);
		TestTrue(
			FString::Printf(TEXT("Great-circle forward %d follows the analytic tangent"), Step),
			GreatCircleForward.Equals(ExpectedGreatCircleForward, 1.0e-4));
		TestTrue(
			FString::Printf(TEXT("Great-circle forward %d is normalized"), Step),
			GreatCircleForward.IsNormalized());
		TestTrue(
			FString::Printf(TEXT("Great-circle right %d is normalized"), Step),
			GreatCircleRight.IsNormalized());
		TestTrue(
			FString::Printf(TEXT("Great-circle forward %d is tangent"), Step),
			FMath::IsNearlyZero(
				FVector::DotProduct(GreatCircleForward, GreatCircleUp),
				1.0e-4));
		TestTrue(
			FString::Printf(TEXT("Great-circle right %d is tangent"), Step),
			FMath::IsNearlyZero(
				FVector::DotProduct(GreatCircleRight, GreatCircleUp),
				1.0e-4));
		TestTrue(
			FString::Printf(TEXT("Great-circle frame %d remains right-handed"), Step),
			FVector::CrossProduct(GreatCircleForward, GreatCircleRight)
				.Equals(GreatCircleUp, 1.0e-4));
		PreviousForward = GreatCircleForward;
	}
	TestTrue(
		TEXT("A complete great-circle traversal returns to its initial forward"),
		PreviousForward.Equals(InitialForward, 1.0e-4));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCharacterMovementGravityContractTest,
	"EverPlanet.Planet.CharacterMovementGravityContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterMovementGravityContractTest::RunTest(const FString& Parameters)
{
	UCharacterMovementComponent* Movement =
		NewObject<UCharacterMovementComponent>(GetTransientPackage());
	TestNotNull(TEXT("A transient Character Movement Component can be created"), Movement);
	if (!Movement)
	{
		return false;
	}

	Movement->SetGravityDirection(FVector(0.0, -17.0, 0.0));
	TestTrue(
		TEXT("SetGravityDirection normalizes its input"),
		Movement->GetGravityDirection().Equals(
			FVector(0.0, -1.0, 0.0),
			UE_KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("A non-default direction enables custom gravity"),
		Movement->HasCustomGravity());

	const FVector ProjectedDirection = Movement->ProjectToGravityFloor(
		FVector(2.0, 3.0, 4.0));
	TestTrue(
		TEXT("ProjectToGravityFloor removes the gravity-axis component"),
		ProjectedDirection.Equals(
			FVector(2.0, 0.0, 4.0),
			UE_KINDA_SMALL_NUMBER));

	FVector GravitySpaceVector(2.0, 3.0, 4.0);
	Movement->SetGravitySpaceZ(GravitySpaceVector, 5.0);
	TestTrue(
		TEXT("SetGravitySpaceZ and GetGravitySpaceZ agree"),
		FMath::IsNearlyEqual(
			Movement->GetGravitySpaceZ(GravitySpaceVector),
			5.0,
			UE_KINDA_SMALL_NUMBER));

	Movement->SetGravityDirection(FVector(0.0, 0.0, -5.0));
	TestFalse(
		TEXT("A scaled world-down direction restores default-gravity state"),
		Movement->HasCustomGravity());

	return true;
}

#endif

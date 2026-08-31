// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

#include "Planet/EverPlanetSphericalCharacter.h"
#include "Planet/PlanetBody.h"
#include "Planet/PlanetGravityComponent.h"

namespace EverPlanetM1RuntimeTest
{
	constexpr TCHAR MapPackage[] =
		TEXT("/Game/EverPlanet/Tests/M1/Maps/Lvl_M1SphericalGravity");
	constexpr TCHAR CharacterClassPath[] =
		TEXT("/Game/EverPlanet/Tests/M1/Blueprints/BP_M1SphericalCharacter.BP_M1SphericalCharacter_C");
	constexpr TCHAR GameModeClassPath[] =
		TEXT("/Game/EverPlanet/Tests/M1/Blueprints/BP_M1SphericalGameMode.BP_M1SphericalGameMode_C");
	constexpr double PlanetRadius = 3000.0;
	constexpr double SpawnClearance = 25.0;
	constexpr double PhaseTimeoutSeconds = 8.0;
	constexpr double WalkDurationSeconds = 1.0;
	constexpr double PostWalkSettleSeconds = 0.1;

	UWorld* FindPIEWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World())
			{
				return Context.World();
			}
		}

		return nullptr;
	}

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}

	class FVerifySphericalRuntimeCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FVerifySphericalRuntimeCommand(FAutomationTestBase* InTest)
			: Test(InTest)
		{
		}

		virtual bool Update() override
		{
			if (!Test)
			{
				return true;
			}

			if (PhaseStartSeconds <= 0.0)
			{
				PhaseStartSeconds = FPlatformTime::Seconds();
			}

			UWorld* World = FindPIEWorld();
			if (!World)
			{
				return WaitOrFail(TEXT("PIE game world was not available."));
			}

			switch (Phase)
			{
			case EPhase::VerifyInitialWorld:
				return VerifyInitialWorld(World);
			case EPhase::WaitForSideLanding:
				return WaitForSideLanding(World);
			case EPhase::WalkOnSide:
				return WalkOnSide(World);
			case EPhase::SettleAfterWalk:
				return SettleAfterWalk(World);
			default:
				return true;
			}
		}

	private:
		enum class EPhase : uint8
		{
			VerifyInitialWorld,
			WaitForSideLanding,
			WalkOnSide,
			SettleAfterWalk,
			Complete
		};

		bool VerifyInitialWorld(UWorld* World)
		{
			const FString PIEPackageName = World->GetOutermost()->GetName();
			const FString SourcePackageName = UWorld::StripPIEPrefixFromPackageName(
				PIEPackageName,
				World->StreamingLevelsPrefix);
			Test->TestEqual(
				TEXT("PIE runs the isolated M1 spherical-gravity map"),
				SourcePackageName,
				FString(MapPackage));

			AGameModeBase* GameMode = World->GetAuthGameMode();
			if (!Test->TestNotNull(TEXT("PIE has an authoritative GameMode"), GameMode))
			{
				return true;
			}
			Test->TestEqual(
				TEXT("The M1 map uses the isolated spherical GameMode"),
				GameMode->GetClass()->GetPathName(),
				FString(GameModeClassPath));

			APlayerController* PlayerController = World->GetFirstPlayerController();
			if (!Test->TestNotNull(TEXT("PIE created a PlayerController"), PlayerController))
			{
				return true;
			}

			AEverPlanetSphericalCharacter* Character =
				Cast<AEverPlanetSphericalCharacter>(PlayerController->GetPawn());
			if (!Test->TestNotNull(TEXT("The PlayerController possesses a spherical character"), Character))
			{
				return true;
			}
			Test->TestEqual(
				TEXT("The possessed pawn uses the isolated M1 character Blueprint"),
				Character->GetClass()->GetPathName(),
				FString(CharacterClassPath));

			UPlanetGravityComponent* GravityComponent =
				Character->GetPlanetGravityComponent();
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			if (!Test->TestNotNull(TEXT("The pawn owns a PlanetGravityComponent"), GravityComponent)
				|| !Test->TestNotNull(TEXT("The pawn owns a CharacterMovementComponent"), Movement))
			{
				return true;
			}

			APlanetBody* Planet = nullptr;
			int32 PlanetCount = 0;
			for (TActorIterator<APlanetBody> It(World); It; ++It)
			{
				Planet = *It;
				++PlanetCount;
			}
			Test->TestEqual(TEXT("The M1 PIE world contains exactly one PlanetBody"), PlanetCount, 1);
			if (!Test->TestNotNull(TEXT("The M1 PIE world contains a PlanetBody"), Planet))
			{
				return true;
			}

			Test->TestTrue(
				TEXT("The authored planet has the expected 3000 cm radius"),
				FMath::IsNearlyEqual(
					static_cast<double>(Planet->GetSurfaceRadius()),
					PlanetRadius,
					UE_KINDA_SMALL_NUMBER));
			Test->TestTrue(
				TEXT("PlanetGravityComponent selected the map's PlanetBody"),
				GravityComponent->GetGravitySource() == Planet);

			FVector GravityDirection = FVector::ZeroVector;
			const bool bHasGravityDirection =
				GravityComponent->TryGetGravityDirection(GravityDirection);
			Test->TestTrue(TEXT("Planet gravity is available at the north-pole spawn"), bHasGravityDirection);
			if (bHasGravityDirection)
			{
				Test->TestTrue(
					TEXT("North-pole gravity points toward the planet center"),
					GravityDirection.Equals(FVector::DownVector, 1.0e-4));
				Test->TestTrue(
					TEXT("CharacterMovement received the north-pole gravity direction"),
					Movement->GetGravityDirection().Equals(GravityDirection, 1.0e-4));
			}

			const double CapsuleHalfHeight =
				static_cast<double>(Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
			SideSpawnDistance = PlanetRadius + CapsuleHalfHeight + SpawnClearance;
			const bool bTeleported = Character->SetActorLocationAndRotation(
				FVector(SideSpawnDistance, 0.0, 0.0),
				FRotator::ZeroRotator,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Test->TestTrue(TEXT("The pawn can be repositioned to the planet side for the runtime probe"), bTeleported);
			Movement->StopMovementImmediately();
			Movement->SetMovementMode(MOVE_Falling);

			CharacterPtr = Character;
			PlanetPtr = Planet;
			Phase = EPhase::WaitForSideLanding;
			PhaseStartSeconds = FPlatformTime::Seconds();
			return false;
		}

		bool WaitForSideLanding(UWorld* World)
		{
			AEverPlanetSphericalCharacter* Character = CharacterPtr.Get();
			APlanetBody* Planet = PlanetPtr.Get();
			if (!Character || !Planet || Character->GetWorld() != World)
			{
				Test->AddError(TEXT("The M1 pawn or planet became invalid while waiting for side landing."));
				return true;
			}

			UPlanetGravityComponent* GravityComponent = Character->GetPlanetGravityComponent();
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			FVector SurfaceNormal = FVector::ZeroVector;
			FVector GravityDirection = FVector::ZeroVector;
			const bool bHasSurfaceNormal =
				GravityComponent && GravityComponent->TryGetSurfaceNormal(SurfaceNormal);
			const bool bHasGravityDirection =
				GravityComponent && GravityComponent->TryGetGravityDirection(GravityDirection);
			const bool bFinite = IsFiniteVector(Character->GetActorLocation())
				&& IsFiniteVector(Character->GetActorUpVector())
				&& IsFiniteVector(SurfaceNormal)
				&& IsFiniteVector(GravityDirection)
				&& Movement
				&& IsFiniteVector(Movement->Velocity);
			if (!bFinite)
			{
				Test->AddError(TEXT("Non-finite transform, gravity, or velocity was produced at the planet side."));
				return true;
			}

			const double CenterDistance = FVector::Distance(
				Character->GetActorLocation(),
				Planet->GetPlanetCenter());
			if (CenterDistance < PlanetRadius - 5.0)
			{
				Test->AddError(TEXT("The pawn penetrated beneath the spherical planet surface."));
				return true;
			}

			const bool bGravityApplied = bHasSurfaceNormal
				&& bHasGravityDirection
				&& Movement
				&& Movement->HasCustomGravity()
				&& GravityDirection.Equals(-SurfaceNormal, 1.0e-4)
				&& Movement->GetGravityDirection().Equals(GravityDirection, 1.0e-4);
			const bool bCapsuleAligned = bHasSurfaceNormal
				&& FVector::DotProduct(Character->GetActorUpVector(), SurfaceNormal) > 0.99;
			const bool bLanded = Movement && Movement->IsMovingOnGround();

			if (bGravityApplied && bCapsuleAligned && bLanded)
			{
				Test->TestTrue(TEXT("Side gravity uses CharacterMovement's custom-gravity path"), true);
				Test->TestTrue(TEXT("The capsule local up aligns with the side surface normal"), true);
				Test->TestTrue(TEXT("The pawn lands on the spherical collision surface"), true);
				const double ExpectedLandedRadius = PlanetRadius
					+ static_cast<double>(Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
				Test->TestTrue(
					TEXT("The landed capsule center is within 10 cm of the expected surface offset"),
					FMath::Abs(CenterDistance - ExpectedLandedRadius) <= 10.0);
				Test->TestTrue(
					TEXT("CharacterMovement reports a walkable spherical floor"),
					Movement->CurrentFloor.IsWalkableFloor());
				Test->TestTrue(
					TEXT("The walkable floor belongs to the authored PlanetBody"),
					Movement->CurrentFloor.HitResult.GetActor() == Planet);
				WalkStartNormal = SurfaceNormal;
				WalkStartRadius = CenterDistance;
				Phase = EPhase::WalkOnSide;
				PhaseStartSeconds = FPlatformTime::Seconds();
				return false;
			}

			return WaitOrFail(
				FString::Printf(
					TEXT("Side landing did not stabilize (gravity=%s, aligned=%s, grounded=%s, radius=%.2f)."),
					bGravityApplied ? TEXT("true") : TEXT("false"),
					bCapsuleAligned ? TEXT("true") : TEXT("false"),
					bLanded ? TEXT("true") : TEXT("false"),
					CenterDistance));
		}

		bool WalkOnSide(UWorld* World)
		{
			AEverPlanetSphericalCharacter* Character = CharacterPtr.Get();
			APlanetBody* Planet = PlanetPtr.Get();
			if (!Character || !Planet || Character->GetWorld() != World)
			{
				Test->AddError(TEXT("The M1 pawn or planet became invalid during tangent movement."));
				return true;
			}
			if (FPlatformTime::Seconds() - PhaseStartSeconds > PhaseTimeoutSeconds)
			{
				Test->AddError(TEXT("Tangent movement did not accumulate one second of PIE game time before the timeout."));
				return true;
			}

			WalkElapsedGameSeconds += FMath::Max(
				static_cast<double>(World->GetDeltaSeconds()),
				0.0);
			if (WalkElapsedGameSeconds < WalkDurationSeconds)
			{
				UPlanetGravityComponent* GravityComponent = Character->GetPlanetGravityComponent();
				UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
				FVector SurfaceNormal = FVector::ZeroVector;
				FVector GravityDirection = FVector::ZeroVector;
				const bool bHasPlanetFrame = GravityComponent
					&& GravityComponent->TryGetSurfaceNormal(SurfaceNormal)
					&& GravityComponent->TryGetGravityDirection(GravityDirection);
				const double CurrentRadius = FVector::Distance(
					Character->GetActorLocation(),
					Planet->GetPlanetCenter());
				const bool bStableFrame = bHasPlanetFrame
					&& Movement
					&& Movement->IsMovingOnGround()
					&& IsFiniteVector(Character->GetActorLocation())
					&& IsFiniteVector(Character->GetActorUpVector())
					&& IsFiniteVector(Movement->Velocity)
					&& FMath::Abs(CurrentRadius - WalkStartRadius) <= 25.0
					&& FVector::DotProduct(Character->GetActorUpVector(), SurfaceNormal) > 0.98
					&& GravityDirection.Equals(-SurfaceNormal, 1.0e-4);
				if (!bStableFrame)
				{
					Test->AddError(
						FString::Printf(
							TEXT("Tangent walking lost a stable spherical frame (radius=%.2f cm)."),
							CurrentRadius));
					return true;
				}
				Character->DoMove(0.0f, 1.0f);
				return false;
			}

			Character->ConsumeMovementInputVector();
			Character->GetCharacterMovement()->StopMovementImmediately();
			Phase = EPhase::SettleAfterWalk;
			PhaseStartSeconds = FPlatformTime::Seconds();
			return false;
		}

		bool SettleAfterWalk(UWorld* World)
		{
			AEverPlanetSphericalCharacter* Character = CharacterPtr.Get();
			APlanetBody* Planet = PlanetPtr.Get();
			if (!Character || !Planet || Character->GetWorld() != World)
			{
				Test->AddError(TEXT("The M1 pawn or planet became invalid after tangent movement."));
				return true;
			}
			if (FPlatformTime::Seconds() - PhaseStartSeconds > PhaseTimeoutSeconds)
			{
				Test->AddError(TEXT("Post-walk PIE stabilization did not complete before the timeout."));
				return true;
			}

			SettleElapsedGameSeconds += FMath::Max(
				static_cast<double>(World->GetDeltaSeconds()),
				0.0);
			if (SettleElapsedGameSeconds < PostWalkSettleSeconds)
			{
				return false;
			}

			UPlanetGravityComponent* GravityComponent = Character->GetPlanetGravityComponent();
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			FVector EndSurfaceNormal = FVector::ZeroVector;
			FVector EndGravityDirection = FVector::ZeroVector;
			const bool bHasEndFrame = GravityComponent
				&& GravityComponent->TryGetSurfaceNormal(EndSurfaceNormal)
				&& GravityComponent->TryGetGravityDirection(EndGravityDirection);
			Test->TestTrue(TEXT("A valid planet frame remains available after tangent movement"), bHasEndFrame);
			if (!bHasEndFrame || !Movement)
			{
				return true;
			}

			const double Dot = FMath::Clamp(
				static_cast<double>(FVector::DotProduct(WalkStartNormal, EndSurfaceNormal)),
				-1.0,
				1.0);
			const double AngularTravelDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));
			const double EndRadius = FVector::Distance(
				Character->GetActorLocation(),
				Planet->GetPlanetCenter());
			const double MovementGravityDot = FMath::Clamp(
				static_cast<double>(FVector::DotProduct(
					Movement->GetGravityDirection(),
					EndGravityDirection)),
				-1.0,
				1.0);
			const double MovementGravityErrorDegrees =
				FMath::RadiansToDegrees(FMath::Acos(MovementGravityDot));

			Test->TestTrue(
				TEXT("Forward movement changes position around the curved surface by at least one degree"),
				AngularTravelDegrees > 1.0);
			Test->TestTrue(
				TEXT("Tangent walking keeps radial distance within 25 cm of the landed value"),
				FMath::Abs(EndRadius - WalkStartRadius) <= 25.0);
			Test->TestTrue(
				TEXT("The pawn remains grounded after tangent walking"),
				Movement->IsMovingOnGround());
			Test->TestTrue(
				TEXT("The capsule remains upright relative to the final surface normal"),
				FVector::DotProduct(Character->GetActorUpVector(), EndSurfaceNormal) > 0.99);
			Test->TestTrue(
				TEXT("CharacterMovement gravity remains opposite the final surface normal"),
				EndGravityDirection.Equals(-EndSurfaceNormal, 1.0e-4)
					&& MovementGravityErrorDegrees <= 0.1);
			Test->TestTrue(
				TEXT("Final transform and velocity remain finite"),
				IsFiniteVector(Character->GetActorLocation())
					&& IsFiniteVector(Character->GetActorUpVector())
					&& IsFiniteVector(Movement->Velocity));

			Test->AddInfo(
				FString::Printf(
					TEXT("M1 tangent walk: angular travel %.3f deg, radius %.2f -> %.2f cm, gravity frame error %.5f deg."),
					AngularTravelDegrees,
					WalkStartRadius,
					EndRadius,
					MovementGravityErrorDegrees));
			Phase = EPhase::Complete;
			return true;
		}

		bool WaitOrFail(const FString& TimeoutMessage)
		{
			if (FPlatformTime::Seconds() - PhaseStartSeconds <= PhaseTimeoutSeconds)
			{
				return false;
			}

			Test->AddError(TimeoutMessage);
			return true;
		}

		FAutomationTestBase* Test = nullptr;
		EPhase Phase = EPhase::VerifyInitialWorld;
		double PhaseStartSeconds = 0.0;
		double SideSpawnDistance = 0.0;
		double WalkStartRadius = 0.0;
		double WalkElapsedGameSeconds = 0.0;
		double SettleElapsedGameSeconds = 0.0;
		FVector WalkStartNormal = FVector::ZeroVector;
		TWeakObjectPtr<AEverPlanetSphericalCharacter> CharacterPtr;
		TWeakObjectPtr<APlanetBody> PlanetPtr;
	};

	class FWaitForPIEEndCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForPIEEndCommand(FAutomationTestBase* InTest)
			: Test(InTest)
		{
		}

		virtual bool Update() override
		{
			if (!FindPIEWorld()
				&& (!GEditor || !GEditor->IsPlaySessionInProgress()))
			{
				return true;
			}

			if (StartSeconds <= 0.0)
			{
				StartSeconds = FPlatformTime::Seconds();
				return false;
			}

			if (FPlatformTime::Seconds() - StartSeconds > PhaseTimeoutSeconds)
			{
				if (Test)
				{
					Test->AddError(TEXT("PIE did not finish shutting down within the timeout."));
				}
				return true;
			}

			return false;
		}

	private:
		FAutomationTestBase* Test = nullptr;
		double StartSeconds = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM1SphericalContentRuntimeTest,
	"EverPlanet.M1.SphericalContentRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM1SphericalContentRuntimeTest::RunTest(const FString& Parameters)
{
	const bool bMapOpenQueued = AutomationOpenMap(EverPlanetM1RuntimeTest::MapPackage, true);
	TestTrue(TEXT("The M1 map load and PIE request was accepted"), bMapOpenQueued);

	ADD_LATENT_AUTOMATION_COMMAND(
		EverPlanetM1RuntimeTest::FVerifySphericalRuntimeCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(
		EverPlanetM1RuntimeTest::FWaitForPIEEndCommand(this));
	return true;
}

#endif

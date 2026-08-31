// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/CapsuleComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Math/RotationMatrix.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

#include "Planet/EverPlanetSphericalCharacter.h"
#include "Planet/PlanetBody.h"
#include "Planet/PlanetGravityComponent.h"

namespace EverPlanetM1MotionEnvelopeTest
{
	constexpr TCHAR MapPackage[] =
		TEXT("/Game/EverPlanet/Tests/M1/Maps/Lvl_M1SphericalGravity");
	constexpr TCHAR CharacterClassPath[] =
		TEXT("/Game/EverPlanet/Tests/M1/Blueprints/BP_M1SphericalCharacter.BP_M1SphericalCharacter_C");
	constexpr double PlanetRadius = 3000.0;
	constexpr double SpawnClearance = 25.0;
	constexpr double PhaseTimeoutSeconds = 8.0;
	constexpr double CardinalWalkDurationSeconds = 0.35;
	constexpr double SettleDurationSeconds = 0.1;
	constexpr double MinimumJumpHeight = 75.0;
	constexpr double MaximumJumpAngularDriftDegrees = 2.0;
	constexpr double ContinuousGravityDotThreshold = 0.9998476951563913; // cos(1 degree)
	constexpr int32 CardinalCount = 6;
	constexpr int32 JumpCardinalIndex = 1;

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

	FVector GetCardinalNormal(const int32 Index)
	{
		switch (Index)
		{
		case 0:
			return FVector::UpVector;
		case 1:
			return FVector::ForwardVector;
		case 2:
			return FVector::RightVector;
		case 3:
			return FVector::DownVector;
		case 4:
			return FVector::BackwardVector;
		case 5:
			return FVector::LeftVector;
		default:
			return FVector::ZeroVector;
		}
	}

	FVector GetCardinalTangent(const int32 Index)
	{
		switch (Index)
		{
		case 0:
		case 2:
		case 3:
		case 5:
			return FVector::ForwardVector;
		case 1:
		case 4:
			return FVector::RightVector;
		default:
			return FVector::ZeroVector;
		}
	}

	const TCHAR* GetCardinalName(const int32 Index)
	{
		switch (Index)
		{
		case 0:
			return TEXT("North(+Z)");
		case 1:
			return TEXT("East(+X)");
		case 2:
			return TEXT("Side(+Y)");
		case 3:
			return TEXT("South(-Z)");
		case 4:
			return TEXT("West(-X)");
		case 5:
			return TEXT("Side(-Y)");
		default:
			return TEXT("Invalid");
		}
	}

	class FVerifySphericalMotionEnvelopeCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FVerifySphericalMotionEnvelopeCommand(FAutomationTestBase* InTest)
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
				return WaitOrFail(TEXT("PIE game world was not available for the motion-envelope test."));
			}

			switch (Phase)
			{
			case EPhase::VerifyWorld:
				return VerifyWorld(World);
			case EPhase::TeleportToCardinal:
				return TeleportToCardinal(World);
			case EPhase::WaitForCardinalLanding:
				return WaitForCardinalLanding(World);
			case EPhase::WalkAtCardinal:
				return WalkAtCardinal(World);
			case EPhase::SettleAfterCardinalWalk:
				return SettleAfterCardinalWalk(World);
			case EPhase::StartJump:
				return StartJump(World);
			case EPhase::TrackJump:
				return TrackJump(World);
			default:
				return true;
			}
		}

	private:
		enum class EPhase : uint8
		{
			VerifyWorld,
			TeleportToCardinal,
			WaitForCardinalLanding,
			WalkAtCardinal,
			SettleAfterCardinalWalk,
			StartJump,
			TrackJump,
			Complete
		};

		bool VerifyWorld(UWorld* World)
		{
			const FString PIEPackageName = World->GetOutermost()->GetName();
			const FString SourcePackageName = UWorld::StripPIEPrefixFromPackageName(
				PIEPackageName,
				World->StreamingLevelsPrefix);
			Test->TestEqual(
				TEXT("The motion-envelope test runs the isolated M1 map"),
				SourcePackageName,
				FString(MapPackage));

			APlayerController* PlayerController = World->GetFirstPlayerController();
			if (!Test->TestNotNull(TEXT("PIE created a PlayerController"), PlayerController))
			{
				return true;
			}

			AEverPlanetSphericalCharacter* Character =
				Cast<AEverPlanetSphericalCharacter>(PlayerController->GetPawn());
			if (!Test->TestNotNull(TEXT("PIE possesses a spherical character"), Character))
			{
				return true;
			}
			Test->TestEqual(
				TEXT("The motion-envelope test uses the M1 character Blueprint"),
				Character->GetClass()->GetPathName(),
				FString(CharacterClassPath));

			APlanetBody* Planet = nullptr;
			int32 PlanetCount = 0;
			for (TActorIterator<APlanetBody> It(World); It; ++It)
			{
				Planet = *It;
				++PlanetCount;
			}
			Test->TestEqual(TEXT("The motion-envelope world contains one PlanetBody"), PlanetCount, 1);
			if (!Test->TestNotNull(TEXT("The motion-envelope world has a PlanetBody"), Planet))
			{
				return true;
			}

			UPlanetGravityComponent* GravityComponent = Character->GetPlanetGravityComponent();
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			if (!Test->TestNotNull(TEXT("The character has a PlanetGravityComponent"), GravityComponent)
				|| !Test->TestNotNull(TEXT("The character has a CharacterMovementComponent"), Movement)
				|| !Test->TestNotNull(TEXT("The character has a CapsuleComponent"), Character->GetCapsuleComponent()))
			{
				return true;
			}

			Test->TestTrue(
				TEXT("The gravity component selected the authored planet"),
				GravityComponent->GetGravitySource() == Planet);
			Test->TestTrue(
				TEXT("The authored planet keeps the expected radius"),
				FMath::IsNearlyEqual(
					static_cast<double>(Planet->GetSurfaceRadius()),
					PlanetRadius,
					UE_KINDA_SMALL_NUMBER));

			CharacterPtr = Character;
			PlanetPtr = Planet;
			TargetCenterRadius = PlanetRadius
				+ static_cast<double>(Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			Phase = EPhase::TeleportToCardinal;
			ResetPhaseClock();
			return false;
		}

		bool TeleportToCardinal(UWorld* World)
		{
			AEverPlanetSphericalCharacter* Character = CharacterPtr.Get();
			APlanetBody* Planet = PlanetPtr.Get();
			if (!ValidateActors(World, Character, Planet, TEXT("teleporting to a cardinal point")))
			{
				return true;
			}

			TargetNormal = GetCardinalNormal(CardinalIndex);
			TargetTangent = GetCardinalTangent(CardinalIndex);
			Test->TestFalse(TEXT("The cardinal test normal is valid"), TargetNormal.IsNearlyZero());
			Test->TestFalse(TEXT("The cardinal test tangent is valid"), TargetTangent.IsNearlyZero());
			Test->TestTrue(
				TEXT("The cardinal test frame is orthogonal"),
				FMath::Abs(FVector::DotProduct(TargetNormal, TargetTangent)) <= 1.0e-4);
			if (TargetNormal.IsNearlyZero()
				|| TargetTangent.IsNearlyZero()
				|| FMath::Abs(FVector::DotProduct(TargetNormal, TargetTangent)) > 1.0e-4)
			{
				return true;
			}

			const FVector TargetLocation = Planet->GetPlanetCenter()
				+ TargetNormal * (TargetCenterRadius + SpawnClearance);
			const FQuat TargetRotation = FRotationMatrix::MakeFromXZ(
				TargetTangent,
				TargetNormal).ToQuat();
			const bool bTeleported = Character->SetActorLocationAndRotation(
				TargetLocation,
				TargetRotation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Test->TestTrue(
				*FString::Printf(TEXT("%s teleport succeeds"), GetCardinalName(CardinalIndex)),
				bTeleported);
			if (!bTeleported)
			{
				return true;
			}

			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			Character->ConsumeMovementInputVector();
			Movement->StopMovementImmediately();
			Movement->SetMovementMode(MOVE_Falling);
			if (AController* Controller = Character->GetController())
			{
				Controller->SetControlRotation(TargetRotation.Rotator());
			}
			Character->ResetPlanetMovementHeading(TargetTangent);

			WalkElapsedGameSeconds = 0.0;
			SettleElapsedGameSeconds = 0.0;
			Phase = EPhase::WaitForCardinalLanding;
			ResetPhaseClock();
			return false;
		}

		bool WaitForCardinalLanding(UWorld* World)
		{
			AEverPlanetSphericalCharacter* Character = CharacterPtr.Get();
			APlanetBody* Planet = PlanetPtr.Get();
			if (!ValidateActors(World, Character, Planet, TEXT("waiting for cardinal landing")))
			{
				return true;
			}

			UPlanetGravityComponent* GravityComponent = Character->GetPlanetGravityComponent();
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			FVector SurfaceNormal = FVector::ZeroVector;
			FVector GravityDirection = FVector::ZeroVector;
			const bool bHasPlanetFrame = GravityComponent
				&& GravityComponent->TryGetSurfaceNormal(SurfaceNormal)
				&& GravityComponent->TryGetGravityDirection(GravityDirection);
			const double CenterRadius = FVector::Distance(
				Character->GetActorLocation(),
				Planet->GetPlanetCenter());
			const bool bExpectedCustomGravity = bHasPlanetFrame
				&& !GravityDirection.Equals(UCharacterMovementComponent::DefaultGravityDirection, 1.0e-4);
			const bool bGravityStable = bHasPlanetFrame
				&& Movement
				&& GravityComponent->GetGravitySource() == Planet
				&& Movement->GetGravityDirection().Equals(GravityDirection, 1.0e-4)
				&& GravityDirection.Equals(-SurfaceNormal, 1.0e-4)
				&& Movement->HasCustomGravity() == bExpectedCustomGravity;
			const bool bAligned = bHasPlanetFrame
				&& FVector::DotProduct(Character->GetActorUpVector(), SurfaceNormal) > 0.99;
			const bool bOnExpectedCardinal = bHasPlanetFrame
				&& FVector::DotProduct(SurfaceNormal, TargetNormal) > 0.999;
			const bool bGroundedOnPlanet = Movement
				&& Movement->IsMovingOnGround()
				&& Movement->CurrentFloor.IsWalkableFloor()
				&& Movement->CurrentFloor.HitResult.GetActor() == Planet;
			const bool bFinite = IsFiniteVector(Character->GetActorLocation())
				&& IsFiniteVector(Character->GetActorUpVector())
				&& IsFiniteVector(SurfaceNormal)
				&& IsFiniteVector(GravityDirection)
				&& Movement
				&& IsFiniteVector(Movement->Velocity);

			if (!bFinite || CenterRadius < PlanetRadius - 5.0)
			{
				Test->AddError(
					FString::Printf(
						TEXT("%s produced a non-finite or penetrating state (radius=%.2f cm)."),
						GetCardinalName(CardinalIndex),
						CenterRadius));
				return true;
			}

			if (bGravityStable && bAligned && bOnExpectedCardinal && bGroundedOnPlanet)
			{
				Test->TestTrue(
					*FString::Printf(TEXT("%s gravity frame is stable"), GetCardinalName(CardinalIndex)),
					true);
				Test->TestTrue(
					*FString::Printf(TEXT("%s capsule aligns with local up"), GetCardinalName(CardinalIndex)),
					true);
				Test->TestTrue(
					*FString::Printf(TEXT("%s lands on the authored planet"), GetCardinalName(CardinalIndex)),
					true);
				Test->TestTrue(
					*FString::Printf(TEXT("%s landed radius is within 10 cm"), GetCardinalName(CardinalIndex)),
					FMath::Abs(CenterRadius - TargetCenterRadius) <= 10.0);
				Test->AddInfo(
					FString::Printf(
						TEXT("M1 cardinal %s landed at radius %.2f cm (customGravity=%s)."),
						GetCardinalName(CardinalIndex),
						CenterRadius,
						Movement->HasCustomGravity() ? TEXT("true") : TEXT("false")));

				WalkStartNormal = SurfaceNormal;
				WalkStartTangent = FVector::VectorPlaneProject(
					TargetTangent,
					SurfaceNormal).GetSafeNormal();
				WalkStartRadius = CenterRadius;
				Phase = EPhase::WalkAtCardinal;
				ResetPhaseClock();
				return false;
			}

			return WaitOrFail(
				FString::Printf(
					TEXT("%s landing did not stabilize (gravity=%s, aligned=%s, cardinal=%s, grounded=%s, radius=%.2f)."),
					GetCardinalName(CardinalIndex),
					bGravityStable ? TEXT("true") : TEXT("false"),
					bAligned ? TEXT("true") : TEXT("false"),
					bOnExpectedCardinal ? TEXT("true") : TEXT("false"),
					bGroundedOnPlanet ? TEXT("true") : TEXT("false"),
					CenterRadius));
		}

		bool WalkAtCardinal(UWorld* World)
		{
			AEverPlanetSphericalCharacter* Character = CharacterPtr.Get();
			APlanetBody* Planet = PlanetPtr.Get();
			if (!ValidateActors(World, Character, Planet, TEXT("walking at a cardinal point")))
			{
				return true;
			}
			if (TimedOut())
			{
				Test->AddError(
					FString::Printf(
						TEXT("%s walk did not accumulate enough PIE game time."),
						GetCardinalName(CardinalIndex)));
				return true;
			}

			WalkElapsedGameSeconds += FMath::Max(
				static_cast<double>(World->GetDeltaSeconds()),
				0.0);
			if (WalkElapsedGameSeconds < CardinalWalkDurationSeconds)
			{
				if (!ValidateStableMotionFrame(Character, Planet, WalkStartRadius))
				{
					Test->AddError(
						FString::Printf(
							TEXT("%s tangent walk lost a stable spherical frame."),
							GetCardinalName(CardinalIndex)));
					return true;
				}

				Character->DoMove(0.0f, 1.0f);
				return false;
			}

			Character->ConsumeMovementInputVector();
			Character->GetCharacterMovement()->StopMovementImmediately();
			Phase = EPhase::SettleAfterCardinalWalk;
			ResetPhaseClock();
			return false;
		}

		bool SettleAfterCardinalWalk(UWorld* World)
		{
			AEverPlanetSphericalCharacter* Character = CharacterPtr.Get();
			APlanetBody* Planet = PlanetPtr.Get();
			if (!ValidateActors(World, Character, Planet, TEXT("settling after a cardinal walk")))
			{
				return true;
			}
			if (TimedOut())
			{
				Test->AddError(
					FString::Printf(
						TEXT("%s post-walk stabilization timed out."),
						GetCardinalName(CardinalIndex)));
				return true;
			}

			SettleElapsedGameSeconds += FMath::Max(
				static_cast<double>(World->GetDeltaSeconds()),
				0.0);
			if (SettleElapsedGameSeconds < SettleDurationSeconds)
			{
				return false;
			}

			FVector EndSurfaceNormal = FVector::ZeroVector;
			const bool bHasEndNormal = Character->GetPlanetGravityComponent()
				&& Character->GetPlanetGravityComponent()->TryGetSurfaceNormal(EndSurfaceNormal);
			const double EndRadius = FVector::Distance(
				Character->GetActorLocation(),
				Planet->GetPlanetCenter());
			const double Dot = bHasEndNormal
				? FMath::Clamp(
					static_cast<double>(FVector::DotProduct(WalkStartNormal, EndSurfaceNormal)),
					-1.0,
					1.0)
				: 1.0;
			const double AngularTravelDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));
			const FVector SignedSurfaceDisplacement = bHasEndNormal
				? FVector::VectorPlaneProject(EndSurfaceNormal, WalkStartNormal).GetSafeNormal()
				: FVector::ZeroVector;
			const double ForwardDisplacementDot = FVector::DotProduct(
				SignedSurfaceDisplacement,
				WalkStartTangent);

			Test->TestTrue(
				*FString::Printf(TEXT("%s keeps a valid surface normal after walking"), GetCardinalName(CardinalIndex)),
				bHasEndNormal);
			Test->TestTrue(
				*FString::Printf(TEXT("%s moves at least 0.5 degrees along the sphere"), GetCardinalName(CardinalIndex)),
				AngularTravelDegrees > 0.5);
			Test->TestTrue(
				*FString::Printf(TEXT("%s moves in the seeded forward tangent direction"), GetCardinalName(CardinalIndex)),
				ForwardDisplacementDot > 0.8);
			Test->TestTrue(
				*FString::Printf(TEXT("%s keeps radial drift within 25 cm"), GetCardinalName(CardinalIndex)),
				FMath::Abs(EndRadius - WalkStartRadius) <= 25.0);
			Test->TestTrue(
				*FString::Printf(TEXT("%s remains grounded after walking"), GetCardinalName(CardinalIndex)),
				Character->GetCharacterMovement()->IsMovingOnGround());
			Test->AddInfo(
				FString::Printf(
					TEXT("M1 cardinal %s walk: %.3f deg, forwardDot %.5f, radius %.2f -> %.2f cm."),
					GetCardinalName(CardinalIndex),
					AngularTravelDegrees,
					ForwardDisplacementDot,
					WalkStartRadius,
					EndRadius));

			if (CardinalIndex == JumpCardinalIndex)
			{
				Phase = EPhase::StartJump;
				ResetPhaseClock();
				return false;
			}

			AdvanceCardinal();
			return Phase == EPhase::Complete;
		}

		bool StartJump(UWorld* World)
		{
			AEverPlanetSphericalCharacter* Character = CharacterPtr.Get();
			APlanetBody* Planet = PlanetPtr.Get();
			if (!ValidateActors(World, Character, Planet, TEXT("starting the side jump")))
			{
				return true;
			}

			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			if (!Test->TestTrue(TEXT("The side jump starts from a grounded state"), Movement->IsMovingOnGround()))
			{
				return true;
			}

			JumpStartRadius = FVector::Distance(
				Character->GetActorLocation(),
				Planet->GetPlanetCenter());
			JumpStartNormal = FVector::ZeroVector;
			if (!Character->GetPlanetGravityComponent()
				|| !Character->GetPlanetGravityComponent()->TryGetSurfaceNormal(JumpStartNormal))
			{
				Test->AddError(TEXT("The side jump could not capture its starting surface normal."));
				return true;
			}
			JumpPeakRadius = JumpStartRadius;
			MaximumJumpAngularDriftObservedDegrees = 0.0;
			bSawJumpFalling = false;
			bSawOutwardMotion = false;
			bSawApex = false;
			bSawDescendingMotion = false;
			bJumpReleased = false;
			Character->DoJumpStart();
			Phase = EPhase::TrackJump;
			ResetPhaseClock();
			return false;
		}

		bool TrackJump(UWorld* World)
		{
			AEverPlanetSphericalCharacter* Character = CharacterPtr.Get();
			APlanetBody* Planet = PlanetPtr.Get();
			if (!ValidateActors(World, Character, Planet, TEXT("tracking the side jump")))
			{
				return true;
			}
			if (TimedOut())
			{
				Test->AddError(TEXT("The side jump did not complete before the timeout."));
				return true;
			}

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
			const double RadialSpeed = Movement && bHasPlanetFrame
				? static_cast<double>(FVector::DotProduct(Movement->Velocity, SurfaceNormal))
				: 0.0;
			JumpPeakRadius = FMath::Max(JumpPeakRadius, CurrentRadius);
			if (bHasPlanetFrame)
			{
				const double CurrentNormalDot = FMath::Clamp(
					static_cast<double>(FVector::DotProduct(JumpStartNormal, SurfaceNormal)),
					-1.0,
					1.0);
				MaximumJumpAngularDriftObservedDegrees = FMath::Max(
					MaximumJumpAngularDriftObservedDegrees,
					FMath::RadiansToDegrees(FMath::Acos(CurrentNormalDot)));
			}

			const bool bStableFrame = bHasPlanetFrame
				&& Movement
				&& GravityComponent->GetGravitySource() == Planet
				&& IsFiniteVector(Character->GetActorLocation())
				&& IsFiniteVector(Character->GetActorUpVector())
				&& IsFiniteVector(Movement->Velocity)
				&& GravityDirection.Equals(-SurfaceNormal, 1.0e-4)
				&& Movement->GetGravityDirection().Equals(GravityDirection, 1.0e-4)
				&& FVector::DotProduct(Character->GetActorUpVector(), SurfaceNormal) > 0.98
				&& CurrentRadius >= TargetCenterRadius - 10.0;
			if (!bStableFrame)
			{
				Test->AddError(
					FString::Printf(
						TEXT("The side jump lost a stable planet frame (radius=%.2f, radialSpeed=%.2f)."),
						CurrentRadius,
						RadialSpeed));
				return true;
			}

			if (Movement->IsFalling())
			{
				bSawJumpFalling = true;
				if (!bJumpReleased)
				{
					Character->DoJumpEnd();
					bJumpReleased = true;
				}
			}
			if (bSawJumpFalling && RadialSpeed > 20.0)
			{
				bSawOutwardMotion = true;
			}
			if (bSawOutwardMotion && RadialSpeed <= 0.0)
			{
				bSawApex = true;
			}
			if (bSawApex && RadialSpeed < -20.0)
			{
				bSawDescendingMotion = true;
			}

			if (bSawJumpFalling
				&& bSawDescendingMotion
				&& Movement->IsMovingOnGround())
			{
				Test->TestTrue(TEXT("The side jump enters Falling"), bSawJumpFalling);
				Test->TestTrue(TEXT("The side jump initially moves outward"), bSawOutwardMotion);
				Test->TestTrue(TEXT("The side jump passes an apex"), bSawApex);
				Test->TestTrue(TEXT("The side jump descends toward the planet"), bSawDescendingMotion);
				Test->TestTrue(
					TEXT("The side jump gains at least 75 cm of radial height"),
					JumpPeakRadius - JumpStartRadius >= MinimumJumpHeight);
				Test->TestTrue(
					TEXT("The side jump stays within 2 degrees of its starting surface normal"),
					MaximumJumpAngularDriftObservedDegrees <= MaximumJumpAngularDriftDegrees);
				Test->TestTrue(
					TEXT("The side jump lands within 10 cm of its starting surface radius"),
					FMath::Abs(CurrentRadius - JumpStartRadius) <= 10.0);
				Test->TestTrue(
					TEXT("The side jump lands on the authored PlanetBody"),
					Movement->CurrentFloor.IsWalkableFloor()
						&& Movement->CurrentFloor.HitResult.GetActor() == Planet);
				Test->AddInfo(
					FString::Printf(
						TEXT("M1 side jump: radius %.2f -> peak %.2f -> landed %.2f cm, angular drift %.4f deg."),
						JumpStartRadius,
						JumpPeakRadius,
						CurrentRadius,
						MaximumJumpAngularDriftObservedDegrees));

				AdvanceCardinal();
				return Phase == EPhase::Complete;
			}

			return false;
		}

		bool ValidateStableMotionFrame(
			AEverPlanetSphericalCharacter* Character,
			APlanetBody* Planet,
			const double ReferenceRadius) const
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
			return bHasPlanetFrame
				&& Movement
				&& GravityComponent->GetGravitySource() == Planet
				&& Movement->IsMovingOnGround()
				&& Movement->CurrentFloor.IsWalkableFloor()
				&& Movement->CurrentFloor.HitResult.GetActor() == Planet
				&& IsFiniteVector(Character->GetActorLocation())
				&& IsFiniteVector(Character->GetActorUpVector())
				&& IsFiniteVector(Movement->Velocity)
				&& GravityDirection.Equals(-SurfaceNormal, 1.0e-4)
				&& FVector::DotProduct(
					Movement->GetGravityDirection(),
					GravityDirection) >= ContinuousGravityDotThreshold
				&& FVector::DotProduct(Character->GetActorUpVector(), SurfaceNormal) > 0.98
				&& FMath::Abs(CurrentRadius - ReferenceRadius) <= 25.0;
		}

		bool ValidateActors(
			UWorld* World,
			AEverPlanetSphericalCharacter* Character,
			APlanetBody* Planet,
			const TCHAR* Context)
		{
			if (Character && Planet && Character->GetWorld() == World && Planet->GetWorld() == World)
			{
				return true;
			}

			Test->AddError(FString::Printf(TEXT("The M1 pawn or planet became invalid while %s."), Context));
			return false;
		}

		void AdvanceCardinal()
		{
			++CardinalIndex;
			if (CardinalIndex >= CardinalCount)
			{
				Phase = EPhase::Complete;
			}
			else
			{
				Phase = EPhase::TeleportToCardinal;
			}
			ResetPhaseClock();
		}

		void ResetPhaseClock()
		{
			PhaseStartSeconds = FPlatformTime::Seconds();
		}

		bool TimedOut() const
		{
			return FPlatformTime::Seconds() - PhaseStartSeconds > PhaseTimeoutSeconds;
		}

		bool WaitOrFail(const FString& TimeoutMessage)
		{
			if (!TimedOut())
			{
				return false;
			}

			Test->AddError(TimeoutMessage);
			return true;
		}

		FAutomationTestBase* Test = nullptr;
		EPhase Phase = EPhase::VerifyWorld;
		int32 CardinalIndex = 0;
		double PhaseStartSeconds = 0.0;
		double TargetCenterRadius = 0.0;
		double WalkStartRadius = 0.0;
		double WalkElapsedGameSeconds = 0.0;
		double SettleElapsedGameSeconds = 0.0;
		double JumpStartRadius = 0.0;
		double JumpPeakRadius = 0.0;
		double MaximumJumpAngularDriftObservedDegrees = 0.0;
		bool bSawJumpFalling = false;
		bool bSawOutwardMotion = false;
		bool bSawApex = false;
		bool bSawDescendingMotion = false;
		bool bJumpReleased = false;
		FVector TargetNormal = FVector::ZeroVector;
		FVector TargetTangent = FVector::ZeroVector;
		FVector WalkStartNormal = FVector::ZeroVector;
		FVector WalkStartTangent = FVector::ZeroVector;
		FVector JumpStartNormal = FVector::ZeroVector;
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
	FM1SphericalMotionEnvelopeRuntimeTest,
	"EverPlanet.M1.SphericalMotionEnvelopeRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM1SphericalMotionEnvelopeRuntimeTest::RunTest(const FString& Parameters)
{
	const bool bMapOpenQueued = AutomationOpenMap(EverPlanetM1MotionEnvelopeTest::MapPackage, true);
	TestTrue(TEXT("The M1 map load and PIE request was accepted for the motion-envelope test"), bMapOpenQueued);

	ADD_LATENT_AUTOMATION_COMMAND(
		EverPlanetM1MotionEnvelopeTest::FVerifySphericalMotionEnvelopeCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(
		EverPlanetM1MotionEnvelopeTest::FWaitForPIEEndCommand(this));
	return true;
}

#endif

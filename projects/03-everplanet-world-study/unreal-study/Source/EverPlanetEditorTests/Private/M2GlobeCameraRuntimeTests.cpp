// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/PlatformTime.h"
#include "Math/RotationMatrix.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

#include "Camera/PlanetGlobeCameraComponent.h"
#include "Planet/PlanetBody.h"
#include "Planet/PlanetGravityComponent.h"
#include "Tests/M2/M2GlobeCameraManualTestCharacter.h"

namespace EverPlanetM2GlobeCameraRuntimeTest
{
	constexpr TCHAR MapPackage[] =
		TEXT("/Game/EverPlanet/Tests/M2/Maps/Lvl_M2GlobeCamera");
	constexpr TCHAR CharacterClassPath[] =
		TEXT("/Game/EverPlanet/Tests/M2/Blueprints/BP_M2GlobeCameraCharacter.BP_M2GlobeCameraCharacter_C");
	constexpr TCHAR GameModeClassPath[] =
		TEXT("/Game/EverPlanet/Tests/M2/Blueprints/BP_M2GlobeCameraGameMode.BP_M2GlobeCameraGameMode_C");

	constexpr double PlanetRadius = 10000.0;
	constexpr double SpawnClearance = 25.0;
	constexpr double CameraArmLength = 1600.0;
	constexpr double CameraDownPitchDegrees = 38.0;
	constexpr double CameraHorizontalFovDegrees = 60.0;
	constexpr double CameraLocalUpOffset = 100.0;
	constexpr double CameraLookAheadDistance = 0.0;
	constexpr double CameraCollisionProbeSize = 18.0;

	constexpr double PhaseTimeoutSeconds = 8.0;
	constexpr double SettleDurationSeconds = 0.2;
	constexpr int32 MinimumSettleFrames = 3;
	constexpr double NumericTolerance = 1.0e-4;
	constexpr double LandedRadiusTolerance = 10.0;
	constexpr double TargetOffsetTolerance = 1.0;
	constexpr double CameraLocationTolerance = 5.0;
	constexpr double CameraRotationToleranceDegrees = 0.5;
	constexpr double CameraPitchToleranceDegrees = 1.0;
	constexpr double CameraRollToleranceDegrees = 1.0;
	constexpr double CameraSurfaceClearance = 100.0;
	constexpr double MovementGravityMinimumDot = 0.9998476951563913; // cos(1 degree)

	struct FCardinalFrame
	{
		const TCHAR* Name;
		FVector SurfaceNormal;
		FVector TangentForward;
		bool bExpectedCustomGravity;
	};

	const FCardinalFrame CardinalFrames[] =
	{
		{ TEXT("North +Z"), FVector::UpVector, FVector::ForwardVector, false },
		{ TEXT("Equator +X"), FVector::ForwardVector, FVector::RightVector, true },
		{ TEXT("Equator +Y"), FVector::RightVector, FVector::ForwardVector, true },
		{ TEXT("South -Z"), FVector::DownVector, FVector::ForwardVector, true },
		{ TEXT("Equator -X"), FVector::BackwardVector, FVector::RightVector, true },
		{ TEXT("Equator -Y"), FVector::LeftVector, FVector::ForwardVector, true }
	};
	constexpr int32 CardinalFrameCount = UE_ARRAY_COUNT(CardinalFrames);

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

	bool IsFiniteRotator(const FRotator& Value)
	{
		return FMath::IsFinite(Value.Pitch)
			&& FMath::IsFinite(Value.Yaw)
			&& FMath::IsFinite(Value.Roll);
	}

	double AngleBetweenDegrees(const FVector& A, const FVector& B)
	{
		const double Dot = FMath::Clamp(
			static_cast<double>(FVector::DotProduct(A, B)),
			-1.0,
			1.0);
		return FMath::RadiansToDegrees(FMath::Acos(Dot));
	}

	class FVerifyGlobeCameraRuntimeCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FVerifyGlobeCameraRuntimeCommand(FAutomationTestBase* InTest)
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
			case EPhase::TeleportToCardinal:
				return TeleportToCardinal(World);
			case EPhase::WaitForCardinalLanding:
				return WaitForCardinalLanding(World);
			case EPhase::SettleAtCardinal:
				return SettleAtCardinal(World);
			default:
				return true;
			}
		}

	private:
		enum class EPhase : uint8
		{
			VerifyInitialWorld,
			TeleportToCardinal,
			WaitForCardinalLanding,
			SettleAtCardinal,
			Complete
		};

		bool VerifyInitialWorld(UWorld* World)
		{
			const FString PIEPackageName = World->GetOutermost()->GetName();
			const FString SourcePackageName = UWorld::StripPIEPrefixFromPackageName(
				PIEPackageName,
				World->StreamingLevelsPrefix);
			Test->TestEqual(
				TEXT("PIE runs the isolated M2 globe-camera map"),
				SourcePackageName,
				FString(MapPackage));

			AGameModeBase* GameMode = World->GetAuthGameMode();
			APlayerController* PlayerController = World->GetFirstPlayerController();
			AM2GlobeCameraManualTestCharacter* Character = PlayerController
				? Cast<AM2GlobeCameraManualTestCharacter>(PlayerController->GetPawn())
				: nullptr;
			APlanetBody* Planet = nullptr;
			int32 PlanetCount = 0;
			for (TActorIterator<APlanetBody> It(World); It; ++It)
			{
				Planet = *It;
				++PlanetCount;
			}

			if (!GameMode
				|| !PlayerController
				|| !PlayerController->PlayerCameraManager
				|| !Character
				|| !Planet
				|| PlanetCount != 1
				|| !Character->GetPlanetGravityComponent()
				|| Character->GetPlanetGravityComponent()->GetGravitySource() != Planet)
			{
				return WaitOrFail(TEXT("M2 GameMode, possessed pawn, camera manager, or gravity source did not become ready."));
			}

			Test->TestEqual(
				TEXT("The M2 map uses the isolated globe-camera GameMode"),
				GameMode->GetClass()->GetPathName(),
				FString(GameModeClassPath));
			Test->TestEqual(
				TEXT("The possessed pawn uses the isolated M2 character Blueprint"),
				Character->GetClass()->GetPathName(),
				FString(CharacterClassPath));
			Test->TestTrue(
				TEXT("The M2 pawn is the controller's active view target"),
				PlayerController->GetViewTarget() == Character);
			Test->TestEqual(
				TEXT("The M2 PIE world contains exactly one PlanetBody"),
				PlanetCount,
				1);
			Test->TestTrue(
				TEXT("The authored M2 planet has the exact 10000 cm radius"),
				FMath::IsNearlyEqual(
					static_cast<double>(Planet->GetSurfaceRadius()),
					PlanetRadius,
					NumericTolerance));

			UPlanetGlobeCameraComponent* CameraRig = Character->GetPlanetGlobeCameraComponent();
			USpringArmComponent* Boom = Character->GetCameraBoom();
			UCameraComponent* Camera = Character->GetFollowCamera();
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
			if (!CameraRig || !Boom || !Camera || !Movement || !Capsule)
			{
				Test->AddError(TEXT("The M2 pawn did not expose all required movement and camera components."));
				return true;
			}

			Test->TestTrue(
				TEXT("The globe-camera rig is registered and ticking in unattended PIE"),
				CameraRig->IsRegistered() && CameraRig->IsComponentTickEnabled());
			Test->TestTrue(
				TEXT("The globe-camera rig ticks after physics"),
				CameraRig->PrimaryComponentTick.TickGroup == TG_PostPhysics);
			Test->TestTrue(TEXT("Reference arm length is exactly 1600 cm"),
				FMath::IsNearlyEqual(CameraRig->GetCameraArmLength(), 1600.0f, 1.0e-4f));
			Test->TestTrue(TEXT("Reference local down-pitch is exactly 38 degrees"),
				FMath::IsNearlyEqual(CameraRig->GetCameraDownPitchDegrees(), 38.0f, 1.0e-4f));
			Test->TestTrue(TEXT("Reference horizontal FOV is exactly 60 degrees"),
				FMath::IsNearlyEqual(CameraRig->GetCameraHorizontalFovDegrees(), 60.0f, 1.0e-4f));
			Test->TestTrue(TEXT("Reference local-up offset is exactly 100 cm"),
				FMath::IsNearlyEqual(CameraRig->GetCameraLocalUpOffset(), 100.0f, 1.0e-4f));
			Test->TestTrue(TEXT("Reference look-ahead is exactly zero"),
				FMath::IsNearlyEqual(CameraRig->GetCameraLookAheadDistance(), 0.0f, 1.0e-4f));

			Test->TestTrue(TEXT("SpringArm receives the exact reference arm length"),
				FMath::IsNearlyEqual(Boom->TargetArmLength, 1600.0f, 1.0e-4f));
			Test->TestFalse(TEXT("SpringArm does not use pawn-control rotation"), Boom->bUsePawnControlRotation);
			Test->TestFalse(TEXT("SpringArm does not inherit pitch"), Boom->bInheritPitch);
			Test->TestFalse(TEXT("SpringArm does not inherit yaw"), Boom->bInheritYaw);
			Test->TestFalse(TEXT("SpringArm does not inherit roll"), Boom->bInheritRoll);
			Test->TestTrue(TEXT("SpringArm owns an absolute world rotation"), Boom->IsUsingAbsoluteRotation());
			Test->TestFalse(TEXT("SpringArm built-in location lag is disabled"), Boom->bEnableCameraLag);
			Test->TestFalse(TEXT("SpringArm built-in rotation lag is disabled"), Boom->bEnableCameraRotationLag);
			Test->TestTrue(TEXT("SpringArm camera collision remains enabled"), Boom->bDoCollisionTest);
			Test->TestTrue(TEXT("SpringArm collision probe is exactly 18 cm"),
				FMath::IsNearlyEqual(Boom->ProbeSize, 18.0f, 1.0e-4f));
			Test->TestTrue(TEXT("SpringArm collision uses ECC_Camera"), Boom->ProbeChannel == ECC_Camera);

			Test->TestTrue(
				TEXT("FollowCamera is attached directly to the globe-camera SpringArm"),
				Camera->GetAttachParent() == Boom);
			Test->TestFalse(TEXT("FollowCamera does not use pawn-control rotation"), Camera->bUsePawnControlRotation);
			Test->TestTrue(
				TEXT("FollowCamera keeps zero relative rotation"),
				Camera->GetRelativeRotation().IsNearlyZero(1.0e-4f));
			Test->TestTrue(
				TEXT("FollowCamera uses perspective projection"),
				Camera->ProjectionMode == ECameraProjectionMode::Perspective);
			Test->TestTrue(TEXT("FollowCamera receives the exact reference FOV"),
				FMath::IsNearlyEqual(Camera->FieldOfView, 60.0f, 1.0e-4f));

			ControllerPtr = PlayerController;
			CharacterPtr = Character;
			PlanetPtr = Planet;
			Phase = EPhase::TeleportToCardinal;
			PhaseStartSeconds = FPlatformTime::Seconds();
			return false;
		}

		bool TeleportToCardinal(UWorld* World)
		{
			APlayerController* PlayerController = ControllerPtr.Get();
			AM2GlobeCameraManualTestCharacter* Character = CharacterPtr.Get();
			APlanetBody* Planet = PlanetPtr.Get();
			if (!ValidateRuntimeObjects(World, PlayerController, Character, Planet))
			{
				return true;
			}

			if (CardinalIndex < 0 || CardinalIndex >= CardinalFrameCount)
			{
				Test->AddError(TEXT("M2 cardinal index escaped the six-frame table."));
				return true;
			}

			const FCardinalFrame& Frame = CardinalFrames[CardinalIndex];
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
			if (!Movement || !Capsule)
			{
				Test->AddError(TEXT("M2 movement or capsule became unavailable before a cardinal teleport."));
				return true;
			}

			const double TargetCenterRadius = PlanetRadius
				+ static_cast<double>(Capsule->GetScaledCapsuleHalfHeight())
				+ SpawnClearance;
			const FVector TargetLocation = Planet->GetPlanetCenter()
				+ Frame.SurfaceNormal * TargetCenterRadius;
			const FQuat TargetRotation = FRotationMatrix::MakeFromXZ(
				Frame.TangentForward,
				Frame.SurfaceNormal).ToQuat();

			Movement->StopMovementImmediately();
			Character->ConsumeMovementInputVector();
			const bool bTeleported = Character->SetActorLocationAndRotation(
				TargetLocation,
				TargetRotation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
			Test->TestTrue(
				*FString::Printf(TEXT("%s can teleport to the exact authored cardinal frame"), Frame.Name),
				bTeleported);
			if (!bTeleported)
			{
				return true;
			}

			Movement->SetMovementMode(MOVE_Falling);
			const bool bHeadingReset = Character->ResetGlobeCameraHeading(
				Frame.TangentForward,
				true);
			Test->TestTrue(
				*FString::Printf(TEXT("%s reseeds and snaps the globe-camera heading"), Frame.Name),
				bHeadingReset);
			if (!bHeadingReset)
			{
				return true;
			}

			Phase = EPhase::WaitForCardinalLanding;
			PhaseStartSeconds = FPlatformTime::Seconds();
			SettleElapsedGameSeconds = 0.0;
			SettleFrameCount = 0;
			return false;
		}

		bool WaitForCardinalLanding(UWorld* World)
		{
			APlayerController* PlayerController = ControllerPtr.Get();
			AM2GlobeCameraManualTestCharacter* Character = CharacterPtr.Get();
			APlanetBody* Planet = PlanetPtr.Get();
			if (!ValidateRuntimeObjects(World, PlayerController, Character, Planet))
			{
				return true;
			}
			if (!VerifyContinuousCameraSafety(PlayerController, Character, Planet))
			{
				return true;
			}

			const FCardinalFrame& Frame = CardinalFrames[CardinalIndex];
			UPlanetGravityComponent* Gravity = Character->GetPlanetGravityComponent();
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			FVector SurfaceNormal = FVector::ZeroVector;
			FVector GravityDirection = FVector::ZeroVector;
			const bool bHasSurfaceNormal = Gravity && Gravity->TryGetSurfaceNormal(SurfaceNormal);
			const bool bHasGravityDirection = Gravity && Gravity->TryGetGravityDirection(GravityDirection);
			const bool bFinite = IsFiniteVector(Character->GetActorLocation())
				&& IsFiniteVector(Character->GetActorUpVector())
				&& IsFiniteVector(SurfaceNormal)
				&& IsFiniteVector(GravityDirection)
				&& Movement
				&& IsFiniteVector(Movement->Velocity);
			if (!bFinite)
			{
				Test->AddError(*FString::Printf(
					TEXT("%s produced a non-finite pawn, gravity, or velocity frame."),
					Frame.Name));
				return true;
			}

			const double CenterDistance = FVector::Distance(
				Character->GetActorLocation(),
				Planet->GetPlanetCenter());
			if (CenterDistance < PlanetRadius - 5.0)
			{
				Test->AddError(*FString::Printf(
					TEXT("%s penetrated beneath the spherical planet surface (radius %.2f cm)."),
					Frame.Name,
					CenterDistance));
				return true;
			}

			const bool bCustomGravityMatches = Movement
				&& Movement->HasCustomGravity() == Frame.bExpectedCustomGravity;
			const bool bGravityApplied = bHasSurfaceNormal
				&& bHasGravityDirection
				&& bCustomGravityMatches
				&& FVector::DotProduct(GravityDirection, -SurfaceNormal) >= 1.0 - NumericTolerance
				&& FVector::DotProduct(Movement->GetGravityDirection(), GravityDirection)
					>= MovementGravityMinimumDot;
			const bool bCapsuleAligned = bHasSurfaceNormal
				&& FVector::DotProduct(Character->GetActorUpVector(), SurfaceNormal) > 0.99;
			const bool bAtTargetCardinal = bHasSurfaceNormal
				&& FVector::DotProduct(SurfaceNormal, Frame.SurfaceNormal) > 0.999;
			const bool bGrounded = Movement && Movement->IsMovingOnGround();
			const bool bWalkablePlanetFloor = bGrounded
				&& Movement->CurrentFloor.IsWalkableFloor()
				&& Movement->CurrentFloor.HitResult.GetActor() == Planet;

			if (bGravityApplied
				&& bCapsuleAligned
				&& bAtTargetCardinal
				&& bWalkablePlanetFloor)
			{
				const double ExpectedLandedRadius = PlanetRadius
					+ static_cast<double>(Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
				Test->TestTrue(
					*FString::Printf(TEXT("%s lands at the expected capsule-center radius"), Frame.Name),
					FMath::Abs(CenterDistance - ExpectedLandedRadius) <= LandedRadiusTolerance);
				Test->TestTrue(
					*FString::Printf(TEXT("%s custom-gravity flag matches its authored frame"), Frame.Name),
					bCustomGravityMatches);
				Test->TestTrue(
					*FString::Printf(TEXT("%s lands on the authored PlanetBody"), Frame.Name),
					bWalkablePlanetFloor);

				Phase = EPhase::SettleAtCardinal;
				PhaseStartSeconds = FPlatformTime::Seconds();
				SettleElapsedGameSeconds = 0.0;
				SettleFrameCount = 0;
				return false;
			}

			return WaitOrFail(FString::Printf(
				TEXT("%s did not stabilize (gravity=%s aligned=%s target=%s floor=%s radius=%.2f)."),
				Frame.Name,
				bGravityApplied ? TEXT("true") : TEXT("false"),
				bCapsuleAligned ? TEXT("true") : TEXT("false"),
				bAtTargetCardinal ? TEXT("true") : TEXT("false"),
				bWalkablePlanetFloor ? TEXT("true") : TEXT("false"),
				CenterDistance));
		}

		bool SettleAtCardinal(UWorld* World)
		{
			APlayerController* PlayerController = ControllerPtr.Get();
			AM2GlobeCameraManualTestCharacter* Character = CharacterPtr.Get();
			APlanetBody* Planet = PlanetPtr.Get();
			if (!ValidateRuntimeObjects(World, PlayerController, Character, Planet))
			{
				return true;
			}
			if (!VerifyContinuousCameraSafety(PlayerController, Character, Planet))
			{
				return true;
			}
			if (FPlatformTime::Seconds() - PhaseStartSeconds > PhaseTimeoutSeconds)
			{
				Test->AddError(*FString::Printf(
					TEXT("%s did not accumulate the required settle time before timeout."),
					CardinalFrames[CardinalIndex].Name));
				return true;
			}

			const FCardinalFrame& Frame = CardinalFrames[CardinalIndex];
			UPlanetGravityComponent* Gravity = Character->GetPlanetGravityComponent();
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			FVector SurfaceNormal = FVector::ZeroVector;
			FVector GravityDirection = FVector::ZeroVector;
			const bool bHasStableFrame = Gravity
				&& Gravity->GetGravitySource() == Planet
				&& Gravity->TryGetSurfaceNormal(SurfaceNormal)
				&& Gravity->TryGetGravityDirection(GravityDirection)
				&& Movement
				&& Movement->HasCustomGravity() == Frame.bExpectedCustomGravity
				&& Movement->IsMovingOnGround()
				&& Movement->CurrentFloor.IsWalkableFloor()
				&& Movement->CurrentFloor.HitResult.GetActor() == Planet
				&& IsFiniteVector(SurfaceNormal)
				&& IsFiniteVector(GravityDirection)
				&& FVector::DotProduct(GravityDirection, -SurfaceNormal) >= 1.0 - NumericTolerance
				&& FVector::DotProduct(Movement->GetGravityDirection(), GravityDirection)
					>= MovementGravityMinimumDot
				&& FVector::DotProduct(Character->GetActorUpVector(), SurfaceNormal) > 0.99;
			if (!bHasStableFrame)
			{
				Test->AddError(*FString::Printf(
					TEXT("%s lost its grounded local-up frame during camera settling."),
					Frame.Name));
				return true;
			}

			SettleElapsedGameSeconds += FMath::Max(
				static_cast<double>(World->GetDeltaSeconds()),
				0.0);
			++SettleFrameCount;
			if (SettleElapsedGameSeconds < SettleDurationSeconds
				|| SettleFrameCount < MinimumSettleFrames)
			{
				return false;
			}

			VerifySettledCardinal(PlayerController, Character, Planet, SurfaceNormal);
			++VerifiedCardinalCount;
			++CardinalIndex;
			if (CardinalIndex >= CardinalFrameCount)
			{
				Test->TestEqual(
					TEXT("All six globe-camera cardinal frames were verified"),
					VerifiedCardinalCount,
					CardinalFrameCount);
				Phase = EPhase::Complete;
				return true;
			}

			Phase = EPhase::TeleportToCardinal;
			PhaseStartSeconds = FPlatformTime::Seconds();
			SettleElapsedGameSeconds = 0.0;
			SettleFrameCount = 0;
			return false;
		}

		void VerifySettledCardinal(
			APlayerController* PlayerController,
			AM2GlobeCameraManualTestCharacter* Character,
			APlanetBody* Planet,
			const FVector& SurfaceNormal)
		{
			const FCardinalFrame& Frame = CardinalFrames[CardinalIndex];
			UPlanetGlobeCameraComponent* CameraRig = Character->GetPlanetGlobeCameraComponent();
			USpringArmComponent* Boom = Character->GetCameraBoom();
			UCameraComponent* Camera = Character->GetFollowCamera();
			if (!CameraRig || !Boom || !Camera || !PlayerController->PlayerCameraManager)
			{
				Test->AddError(*FString::Printf(
					TEXT("%s lost a required camera object before the settled sample."),
					Frame.Name));
				return;
			}

			FVector PovLocation = FVector::ZeroVector;
			FRotator PovRotation = FRotator::ZeroRotator;
			PlayerController->GetPlayerViewPoint(PovLocation, PovRotation);
			const FVector PovForward = FRotationMatrix(PovRotation).GetUnitAxis(EAxis::X);
			const FVector PovUp = FRotationMatrix(PovRotation).GetUnitAxis(EAxis::Z);
			const FVector ProjectedSurfaceUp = FVector::VectorPlaneProject(
				SurfaceNormal,
				PovForward).GetSafeNormal();
			const double ActualDownPitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(
				-static_cast<double>(FVector::DotProduct(PovForward, SurfaceNormal)),
				-1.0,
				1.0)));
			const double RollErrorDegrees = !ProjectedSurfaceUp.IsNearlyZero()
				&& !PovUp.IsNearlyZero()
				? AngleBetweenDegrees(ProjectedSurfaceUp, PovUp)
				: 180.0;

			Test->TestTrue(
				*FString::Printf(TEXT("%s actual POV is finite"), Frame.Name),
				IsFiniteVector(PovLocation)
					&& IsFiniteRotator(PovRotation)
					&& IsFiniteVector(PovForward)
					&& IsFiniteVector(PovUp));
			Test->TestTrue(
				*FString::Printf(TEXT("%s actual POV local pitch matches 38 degrees"), Frame.Name),
				FMath::Abs(ActualDownPitch - CameraDownPitchDegrees)
					<= CameraPitchToleranceDegrees);
			Test->TestTrue(
				*FString::Printf(TEXT("%s actual POV has at most one degree of local roll"), Frame.Name),
				RollErrorDegrees <= CameraRollToleranceDegrees);

			const FVector RigSurfaceNormal = CameraRig->GetCameraSurfaceNormal();
			const FVector RigTangentForward = CameraRig->GetCameraTangentForward();
			Test->TestTrue(
				*FString::Printf(TEXT("%s rig surface normal matches gravity local up"), Frame.Name),
				IsFiniteVector(RigSurfaceNormal)
					&& FVector::DotProduct(RigSurfaceNormal, SurfaceNormal) > 0.9999);
			Test->TestTrue(
				*FString::Printf(TEXT("%s rig preserves the authored tangent heading"), Frame.Name),
				IsFiniteVector(RigTangentForward)
					&& FVector::DotProduct(RigTangentForward, Frame.TangentForward) > 0.999);

			const FVector ExpectedTargetOffset =
				SurfaceNormal * CameraLocalUpOffset
				+ RigTangentForward * CameraLookAheadDistance;
			const FVector ActualTargetOffset = Boom->TargetOffset;
			const double ActualUpOffset = FVector::DotProduct(
				ActualTargetOffset,
				SurfaceNormal);
			const FVector ActualTangentialOffset = ActualTargetOffset
				- SurfaceNormal * ActualUpOffset;
			const FVector ExpectedTangentialOffset =
				RigTangentForward * CameraLookAheadDistance;
			Test->TestTrue(
				*FString::Printf(TEXT("%s TargetOffset is finite"), Frame.Name),
				IsFiniteVector(ActualTargetOffset));
			Test->TestTrue(
				*FString::Printf(TEXT("%s TargetOffset follows local up and tangent"), Frame.Name),
				FVector::Distance(ActualTargetOffset, ExpectedTargetOffset)
					<= TargetOffsetTolerance);
			Test->TestTrue(
				*FString::Printf(TEXT("%s TargetOffset keeps the exact 100 cm local-up component"), Frame.Name),
				FMath::Abs(ActualUpOffset - CameraLocalUpOffset)
					<= TargetOffsetTolerance);
			Test->TestTrue(
				*FString::Printf(TEXT("%s TargetOffset keeps zero tangential look-ahead"), Frame.Name),
				FVector::Distance(ActualTangentialOffset, ExpectedTangentialOffset)
					<= TargetOffsetTolerance);

			const FVector EffectivePivot = Boom->GetComponentLocation() + Boom->TargetOffset;
			const FVector CameraLocation = Camera->GetComponentLocation();
			const double ActualArmLength = FVector::Distance(EffectivePivot, CameraLocation);
			const double PovRadius = FVector::Distance(PovLocation, Planet->GetPlanetCenter());
			const double CameraRadius = FVector::Distance(CameraLocation, Planet->GetPlanetCenter());
			Test->TestTrue(
				*FString::Printf(TEXT("%s camera component remains outside the planet"), Frame.Name),
				CameraRadius >= PlanetRadius + CameraSurfaceClearance);
			Test->TestTrue(
				*FString::Printf(TEXT("%s actual player POV remains outside the planet"), Frame.Name),
				PovRadius >= PlanetRadius + CameraSurfaceClearance);
			Test->TestTrue(
				*FString::Printf(TEXT("%s empty-map camera keeps the full 1600 cm arm"), Frame.Name),
				FMath::Abs(ActualArmLength - CameraArmLength) <= CameraLocationTolerance);
			Test->TestFalse(
				*FString::Printf(TEXT("%s empty M2 map does not spuriously apply a collision fix"), Frame.Name),
				Boom->IsCollisionFixApplied());

			Test->TestTrue(
				*FString::Printf(TEXT("%s actual player POV location equals FollowCamera"), Frame.Name),
				FVector::Distance(PovLocation, CameraLocation) <= CameraLocationTolerance);
			const double PovCameraRotationErrorDegrees = FMath::RadiansToDegrees(
				PovRotation.Quaternion().AngularDistance(Camera->GetComponentQuat()));
			Test->TestTrue(
				*FString::Printf(TEXT("%s actual player POV rotation equals FollowCamera"), Frame.Name),
				PovCameraRotationErrorDegrees <= CameraRotationToleranceDegrees);
			Test->TestTrue(
				*FString::Printf(TEXT("%s actual PlayerCameraManager FOV is 60 degrees"), Frame.Name),
				FMath::Abs(
					static_cast<double>(PlayerController->PlayerCameraManager->GetFOVAngle())
					- CameraHorizontalFovDegrees) <= 0.1);

			Test->AddInfo(FString::Printf(
				TEXT("M2 cardinal %s: actual pitch %.3f deg, roll %.3f deg, arm %.2f cm, POV radius %.2f cm, TargetOffset %s."),
				Frame.Name,
				ActualDownPitch,
				RollErrorDegrees,
				ActualArmLength,
				PovRadius,
				*ActualTargetOffset.ToCompactString()));
		}

		bool ValidateRuntimeObjects(
			UWorld* World,
			APlayerController* PlayerController,
			AM2GlobeCameraManualTestCharacter* Character,
			APlanetBody* Planet)
		{
			if (!PlayerController || !Character || !Planet)
			{
				Test->AddError(TEXT("The M2 controller, pawn, or planet became invalid during the cardinal sweep."));
				return false;
			}
			if (PlayerController->GetWorld() != World
				|| Character->GetWorld() != World
				|| Planet->GetWorld() != World)
			{
				Test->AddError(TEXT("An M2 cardinal runtime object no longer belongs to the active PIE world."));
				return false;
			}
			if (PlayerController->GetPawn() != Character
				|| PlayerController->GetViewTarget() != Character)
			{
				Test->AddError(TEXT("The M2 pawn lost possession or stopped being the active player view target."));
				return false;
			}
			return true;
		}

		bool VerifyContinuousCameraSafety(
			APlayerController* PlayerController,
			AM2GlobeCameraManualTestCharacter* Character,
			APlanetBody* Planet)
		{
			UPlanetGlobeCameraComponent* CameraRig = Character->GetPlanetGlobeCameraComponent();
			USpringArmComponent* Boom = Character->GetCameraBoom();
			UCameraComponent* Camera = Character->GetFollowCamera();
			if (!CameraRig || !Boom || !Camera || !PlayerController->PlayerCameraManager)
			{
				Test->AddError(TEXT("A required M2 camera object became unavailable during teleport/settle."));
				return false;
			}

			FVector PovLocation = FVector::ZeroVector;
			FRotator PovRotation = FRotator::ZeroRotator;
			PlayerController->GetPlayerViewPoint(PovLocation, PovRotation);
			const FVector CameraLocation = Camera->GetComponentLocation();
			const bool bFinite = IsFiniteVector(PovLocation)
				&& IsFiniteRotator(PovRotation)
				&& IsFiniteVector(CameraLocation)
				&& IsFiniteVector(Camera->GetForwardVector())
				&& IsFiniteVector(Camera->GetUpVector())
				&& IsFiniteVector(Boom->TargetOffset)
				&& IsFiniteVector(CameraRig->GetCameraSurfaceNormal())
				&& IsFiniteVector(CameraRig->GetCameraTangentForward());
			if (!bFinite)
			{
				Test->AddError(*FString::Printf(
					TEXT("%s produced a non-finite actual POV or camera-rig frame."),
					CardinalFrames[CardinalIndex].Name));
				return false;
			}

			const double PovRadius = FVector::Distance(PovLocation, Planet->GetPlanetCenter());
			const double CameraRadius = FVector::Distance(CameraLocation, Planet->GetPlanetCenter());
			if (PovRadius < PlanetRadius + CameraSurfaceClearance
				|| CameraRadius < PlanetRadius + CameraSurfaceClearance)
			{
				Test->AddError(*FString::Printf(
					TEXT("%s moved the actual POV or FollowCamera inside the safety shell (POV %.2f, component %.2f cm)."),
					CardinalFrames[CardinalIndex].Name,
					PovRadius,
					CameraRadius));
				return false;
			}

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
		double SettleElapsedGameSeconds = 0.0;
		int32 SettleFrameCount = 0;
		int32 CardinalIndex = 0;
		int32 VerifiedCardinalCount = 0;
		TWeakObjectPtr<APlayerController> ControllerPtr;
		TWeakObjectPtr<AM2GlobeCameraManualTestCharacter> CharacterPtr;
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
					Test->AddError(TEXT("M2 PIE did not finish shutting down within the timeout."));
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
	FM2GlobeCameraCardinalRuntimeTest,
	"EverPlanet.M2.GlobeCameraCardinalRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM2GlobeCameraCardinalRuntimeTest::RunTest(const FString& Parameters)
{
	const bool bMapOpenQueued = AutomationOpenMap(
		EverPlanetM2GlobeCameraRuntimeTest::MapPackage,
		true);
	TestTrue(TEXT("The M2 map load and PIE request was accepted"), bMapOpenQueued);

	ADD_LATENT_AUTOMATION_COMMAND(
		EverPlanetM2GlobeCameraRuntimeTest::FVerifyGlobeCameraRuntimeCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(
		EverPlanetM2GlobeCameraRuntimeTest::FWaitForPIEEndCommand(this));
	return true;
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/EverPlanetGlobeCameraCharacter.h"
#include "Camera/PlanetGlobeCameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Planet/PlanetBody.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "World/Data/EverPlanetWorldCatalog.h"
#include "World/Travel/EverPlanetRegionDescriptor.h"
#include "World/Travel/EverPlanetRegionPlayerStart.h"
#include "World/Travel/EverPlanetRegionPortal.h"

#include "EverPlanetGameMode.h"

namespace EverPlanetFirstPlanetRegionalContentTest
{
	constexpr TCHAR CatalogPackage[] =
		TEXT("/Game/EverPlanet/Product/World/DA_EverPlanetWorldCatalog");
	constexpr TCHAR CatalogObjectPath[] =
		TEXT("/Game/EverPlanet/Product/World/DA_EverPlanetWorldCatalog.DA_EverPlanetWorldCatalog");
	constexpr TCHAR ProductCharacterPackage[] =
		TEXT("/Game/EverPlanet/Product/Characters/BP_EverPlanetPlayerCharacter");
	constexpr TCHAR ProductCharacterClassPath[] =
		TEXT("/Game/EverPlanet/Product/Characters/BP_EverPlanetPlayerCharacter.BP_EverPlanetPlayerCharacter_C");
	constexpr TCHAR ProductGameModePackage[] =
		TEXT("/Game/EverPlanet/Product/GameModes/BP_EverPlanetGameMode");
	constexpr TCHAR ProductGameModeClassPath[] =
		TEXT("/Game/EverPlanet/Product/GameModes/BP_EverPlanetGameMode.BP_EverPlanetGameMode_C");
	constexpr TCHAR RegionalGameModePackage[] =
		TEXT("/Game/EverPlanet/Product/GameModes/BP_EverPlanetRegionalGameMode");
	constexpr TCHAR RegionalGameModeClassPath[] =
		TEXT("/Game/EverPlanet/Product/GameModes/BP_EverPlanetRegionalGameMode.BP_EverPlanetRegionalGameMode_C");
	constexpr TCHAR TownMapPackage[] =
		TEXT("/Game/EverPlanet/Product/Planets/FirstPlanet/Maps/Lvl_FirstPlanet_Town");
	constexpr TCHAR FieldMapPackage[] =
		TEXT("/Game/EverPlanet/Product/Planets/FirstPlanet/Maps/Lvl_FirstPlanet_Field");

	const FName PlanetId(TEXT("planet.first"));
	const FName TownRegionId(TEXT("region.first.town"));
	const FName FieldRegionId(TEXT("region.first.field"));
	const FName TownStartAnchorId(TEXT("spawn.town.start"));
	const FName TownFromFieldAnchorId(TEXT("spawn.town.from_field"));
	const FName FieldFromTownAnchorId(TEXT("spawn.field.from_town"));
	const FName TownPortalId(TEXT("portal.town.east_gate"));
	const FName FieldPortalId(TEXT("portal.field.west_gate"));
	const FName TownToFieldLinkId(TEXT("link.first.town_to_field"));
	const FName FieldToTownLinkId(TEXT("link.first.field_to_town"));
	const FName FieldVisitedQuestFlag(TEXT("quest.first.field_visited"));

	constexpr double PlanetRadius = 10000.0;
	constexpr double SpawnClearance = 25.0;
	constexpr double KillZ = -1000000.0;
	constexpr double MinimumPortalAnchorArc = 600.0;
	constexpr double TransformTolerance = 0.1;
	constexpr double RotationTolerance = 0.01;

	struct FStartContract
	{
		const TCHAR* Label;
		FName SpawnAnchorId;
		double AngleDegrees;
	};

	struct FMapContract
	{
		const TCHAR* MapPackage;
		FName RegionId;
		int32 ActorCount;
		int32 StaticMeshActorCount;
		int32 RegionalStartCount;
		const TCHAR* DescriptorLabel;
		const TCHAR* PortalLabel;
		FName PortalId;
		FName TravelLinkId;
		double PortalAngleDegrees;
		TArray<FString> ExpectedLabels;
		TArray<FStartContract> StartContracts;
	};

	bool RequirePackage(
		FAutomationTestBase& Test,
		const TCHAR* LongPackageName,
		const TCHAR* Description)
	{
		if (FPackageName::DoesPackageExist(LongPackageName))
		{
			return true;
		}

		Test.AddError(FString::Printf(
			TEXT("Required generated %s package is missing: %s"),
			Description,
			LongPackageName));
		return false;
	}

	bool VerifyAddress(
		FAutomationTestBase& Test,
		const TCHAR* Description,
		const FEverPlanetWorldAddress& Address,
		const FName ExpectedPlanetId,
		const FName ExpectedRegionId,
		const FName ExpectedAnchorId)
	{
		bool bMatches = true;
		bMatches &= Test.TestEqual(
			FString::Printf(TEXT("%s PlanetId"), Description),
			Address.PlanetId,
			ExpectedPlanetId);
		bMatches &= Test.TestEqual(
			FString::Printf(TEXT("%s RegionId"), Description),
			Address.RegionId,
			ExpectedRegionId);
		bMatches &= Test.TestEqual(
			FString::Printf(TEXT("%s SpawnAnchorId"), Description),
			Address.SpawnAnchorId,
			ExpectedAnchorId);
		return bMatches;
	}

	bool VerifyCatalog(
		FAutomationTestBase& Test,
		const UEverPlanetWorldCatalog& Catalog)
	{
		EEverPlanetWorldFailure Failure = EEverPlanetWorldFailure::None;
		FString Error;
		bool bValid = Catalog.ValidateCatalogDetailed(Failure, Error);
		Test.TestTrue(
			FString::Printf(TEXT("Generated Catalog validates: %s"), *Error),
			bValid);
		Test.TestEqual(
			TEXT("Generated Catalog validation reports no failure"),
			Failure,
			EEverPlanetWorldFailure::None);

		VerifyAddress(
			Test,
			TEXT("InitialAddress"),
			Catalog.InitialAddress,
			PlanetId,
			TownRegionId,
			TownStartAnchorId);

		if (!Test.TestEqual(TEXT("Catalog contains exactly one Planet"), Catalog.Planets.Num(), 1)
			|| !Test.TestEqual(TEXT("Catalog contains exactly two Regions"), Catalog.Regions.Num(), 2)
			|| !Test.TestEqual(TEXT("Catalog contains exactly two TravelLinks"), Catalog.TravelLinks.Num(), 2))
		{
			return false;
		}

		const FEverPlanetPlanetDefinition& Planet = Catalog.Planets[0];
		Test.TestEqual(TEXT("First Planet stable ID"), Planet.PlanetId, PlanetId);
		Test.TestEqual(TEXT("First Planet display name"), Planet.DisplayName.ToString(), FString(TEXT("First Planet")));
		Test.TestEqual(TEXT("First Planet default Region"), Planet.DefaultRegionId, TownRegionId);
		if (Test.TestEqual(TEXT("First Planet owns exactly two Region IDs"), Planet.RegionIds.Num(), 2))
		{
			Test.TestEqual(TEXT("First Planet RegionIds[0] is Town"), Planet.RegionIds[0], TownRegionId);
			Test.TestEqual(TEXT("First Planet RegionIds[1] is Field"), Planet.RegionIds[1], FieldRegionId);
		}

		const FEverPlanetRegionDefinition& Town = Catalog.Regions[0];
		const FEverPlanetRegionDefinition& Field = Catalog.Regions[1];
		Test.TestEqual(TEXT("Town owner Planet"), Town.PlanetId, PlanetId);
		Test.TestEqual(TEXT("Town stable ID"), Town.RegionId, TownRegionId);
		Test.TestEqual(TEXT("Town display name"), Town.DisplayName.ToString(), FString(TEXT("Town")));
		Test.TestEqual(TEXT("Town map package"), Town.Map.ToSoftObjectPath().GetLongPackageName(), FString(TownMapPackage));
		Test.TestEqual(TEXT("Town default anchor"), Town.DefaultSpawnAnchorId, TownStartAnchorId);
		if (Test.TestEqual(TEXT("Town declares exactly two anchors"), Town.SpawnAnchorIds.Num(), 2))
		{
			Test.TestEqual(TEXT("Town anchor[0] is initial"), Town.SpawnAnchorIds[0], TownStartAnchorId);
			Test.TestEqual(TEXT("Town anchor[1] is Field return"), Town.SpawnAnchorIds[1], TownFromFieldAnchorId);
		}

		Test.TestEqual(TEXT("Field owner Planet"), Field.PlanetId, PlanetId);
		Test.TestEqual(TEXT("Field stable ID"), Field.RegionId, FieldRegionId);
		Test.TestEqual(TEXT("Field display name"), Field.DisplayName.ToString(), FString(TEXT("Field")));
		Test.TestEqual(TEXT("Field map package"), Field.Map.ToSoftObjectPath().GetLongPackageName(), FString(FieldMapPackage));
		Test.TestEqual(TEXT("Field default anchor"), Field.DefaultSpawnAnchorId, FieldFromTownAnchorId);
		if (Test.TestEqual(TEXT("Field declares exactly one anchor"), Field.SpawnAnchorIds.Num(), 1))
		{
			Test.TestEqual(TEXT("Field anchor[0] is Town arrival"), Field.SpawnAnchorIds[0], FieldFromTownAnchorId);
		}

		const FEverPlanetTravelLinkDefinition& ToField = Catalog.TravelLinks[0];
		Test.TestEqual(TEXT("Town to Field LinkId"), ToField.LinkId, TownToFieldLinkId);
		Test.TestEqual(TEXT("Town to Field Scope"), ToField.Scope, EEverPlanetTravelScope::IntraPlanet);
		Test.TestEqual(TEXT("Town to Field source Planet"), ToField.Source.PlanetId, PlanetId);
		Test.TestEqual(TEXT("Town to Field source Region"), ToField.Source.RegionId, TownRegionId);
		Test.TestEqual(TEXT("Town to Field source Portal"), ToField.SourcePortalId, TownPortalId);
		VerifyAddress(
			Test,
			TEXT("Town to Field destination"),
			ToField.Destination,
			PlanetId,
			FieldRegionId,
			FieldFromTownAnchorId);
		Test.TestTrue(TEXT("Town to Field starts unlocked"), ToField.bInitiallyUnlocked);
		Test.TestEqual(TEXT("Town to Field has no required quest"), ToField.RequiredQuestFlag, NAME_None);
		Test.TestEqual(TEXT("Town to Field grants visited quest"), ToField.ArrivalQuestFlag, FieldVisitedQuestFlag);

		const FEverPlanetTravelLinkDefinition& ToTown = Catalog.TravelLinks[1];
		Test.TestEqual(TEXT("Field to Town LinkId"), ToTown.LinkId, FieldToTownLinkId);
		Test.TestEqual(TEXT("Field to Town Scope"), ToTown.Scope, EEverPlanetTravelScope::IntraPlanet);
		Test.TestEqual(TEXT("Field to Town source Planet"), ToTown.Source.PlanetId, PlanetId);
		Test.TestEqual(TEXT("Field to Town source Region"), ToTown.Source.RegionId, FieldRegionId);
		Test.TestEqual(TEXT("Field to Town source Portal"), ToTown.SourcePortalId, FieldPortalId);
		VerifyAddress(
			Test,
			TEXT("Field to Town destination"),
			ToTown.Destination,
			PlanetId,
			TownRegionId,
			TownFromFieldAnchorId);
		Test.TestFalse(TEXT("Field to Town starts locked"), ToTown.bInitiallyUnlocked);
		Test.TestEqual(TEXT("Field to Town requires visited quest"), ToTown.RequiredQuestFlag, FieldVisitedQuestFlag);
		Test.TestEqual(TEXT("Field to Town grants no extra quest"), ToTown.ArrivalQuestFlag, NAME_None);
		return bValid;
	}

	bool ReadBoolProperty(
		FAutomationTestBase& Test,
		const UObject* Container,
		const UClass* OwnerClass,
		const TCHAR* PropertyName,
		bool& OutValue)
	{
		const FBoolProperty* Property = FindFProperty<FBoolProperty>(OwnerClass, PropertyName);
		if (!Property || !Container)
		{
			Test.AddError(FString::Printf(TEXT("Required bool property is unavailable: %s"), PropertyName));
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(Container);
		return true;
	}

	bool ReadFloatProperty(
		FAutomationTestBase& Test,
		const UObject* Container,
		const UClass* OwnerClass,
		const TCHAR* PropertyName,
		float& OutValue)
	{
		const FFloatProperty* Property = FindFProperty<FFloatProperty>(OwnerClass, PropertyName);
		if (!Property || !Container)
		{
			Test.AddError(FString::Printf(TEXT("Required float property is unavailable: %s"), PropertyName));
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(Container);
		return true;
	}

	bool VerifyClasses(
		FAutomationTestBase& Test,
		UClass*& OutRegionalGameModeClass,
		double& OutSpawnDistance)
	{
		UClass* ProductGameModeClass = StaticLoadClass(
			AGameModeBase::StaticClass(),
			nullptr,
			ProductGameModeClassPath);
		OutRegionalGameModeClass = StaticLoadClass(
			AGameModeBase::StaticClass(),
			nullptr,
			RegionalGameModeClassPath);
		UClass* ProductCharacterClass = StaticLoadClass(
			AEverPlanetGlobeCameraCharacter::StaticClass(),
			nullptr,
			ProductCharacterClassPath);
		if (!ProductGameModeClass || !OutRegionalGameModeClass || !ProductCharacterClass)
		{
			Test.AddError(TEXT("One or more required Product Blueprint generated classes could not be loaded."));
			return false;
		}

		Test.TestEqual(
			TEXT("Regional GameMode has the exact Product GameMode Blueprint parent"),
			OutRegionalGameModeClass->GetSuperClass(),
			ProductGameModeClass);
		Test.TestEqual(
			TEXT("Regional GameMode generated class path"),
			OutRegionalGameModeClass->GetPathName(),
			FString(RegionalGameModeClassPath));
		Test.TestEqual(
			TEXT("Product Character generated class path"),
			ProductCharacterClass->GetPathName(),
			FString(ProductCharacterClassPath));

		const AEverPlanetGameMode* ProductGameModeDefaults =
			Cast<AEverPlanetGameMode>(ProductGameModeClass->GetDefaultObject());
		const AEverPlanetGameMode* RegionalGameModeDefaults =
			Cast<AEverPlanetGameMode>(OutRegionalGameModeClass->GetDefaultObject());
		if (!ProductGameModeDefaults || !RegionalGameModeDefaults)
		{
			Test.AddError(TEXT("Product GameMode class defaults are unavailable."));
			return false;
		}

		bool bProductRegional = true;
		bool bChildRegional = false;
		if (ReadBoolProperty(
				Test,
				ProductGameModeDefaults,
				AEverPlanetGameMode::StaticClass(),
				TEXT("bRequireRegionalSpawnFlow"),
				bProductRegional)
			&& ReadBoolProperty(
				Test,
				RegionalGameModeDefaults,
				AEverPlanetGameMode::StaticClass(),
				TEXT("bRequireRegionalSpawnFlow"),
				bChildRegional))
		{
			Test.TestFalse(TEXT("Product base GameMode keeps regional flow disabled"), bProductRegional);
			Test.TestTrue(TEXT("Regional child GameMode enables regional flow"), bChildRegional);
		}

		Test.TestEqual(
			TEXT("Regional GameMode uses the exact fixed Product Character"),
			RegionalGameModeDefaults->DefaultPawnClass.Get(),
			ProductCharacterClass);
		Test.TestEqual(
			TEXT("Regional GameMode inherits the Product PlayerController"),
			RegionalGameModeDefaults->PlayerControllerClass.Get(),
			ProductGameModeDefaults->PlayerControllerClass.Get());

		const AEverPlanetGlobeCameraCharacter* CharacterDefaults =
			Cast<AEverPlanetGlobeCameraCharacter>(ProductCharacterClass->GetDefaultObject());
		if (!CharacterDefaults)
		{
			Test.AddError(TEXT("Fixed Product Character class defaults are unavailable."));
			return false;
		}

		const UPlanetGlobeCameraComponent* CameraRig =
			CharacterDefaults->GetPlanetGlobeCameraComponent();
		const UCapsuleComponent* Capsule = CharacterDefaults->GetCapsuleComponent();
		if (!CameraRig || !Capsule)
		{
			Test.AddError(TEXT("Fixed Product Character camera or capsule component is unavailable."));
			return false;
		}

		float CameraYaw = -1.0f;
		float CameraPitchInput = -1.0f;
		if (ReadFloatProperty(
				Test,
				CameraRig,
				UPlanetGlobeCameraComponent::StaticClass(),
				TEXT("CameraYawDegreesPerInputUnit"),
				CameraYaw)
			&& ReadFloatProperty(
				Test,
				CameraRig,
				UPlanetGlobeCameraComponent::StaticClass(),
				TEXT("CameraPitchDegreesPerInputUnit"),
				CameraPitchInput))
		{
			Test.TestTrue(TEXT("Product camera yaw input is fixed at zero"), FMath::IsNearlyZero(CameraYaw));
			Test.TestTrue(TEXT("Product camera pitch input is fixed at zero"), FMath::IsNearlyZero(CameraPitchInput));
		}
		Test.TestTrue(
			TEXT("Product camera arm is 1600 cm"),
			FMath::IsNearlyEqual(CameraRig->GetCameraArmLength(), 1600.0f));
		Test.TestTrue(
			TEXT("Product camera down pitch is 38 degrees"),
			FMath::IsNearlyEqual(CameraRig->GetCameraDownPitchDegrees(), 38.0f));
		Test.TestTrue(
			TEXT("Product camera horizontal FOV is 60 degrees"),
			FMath::IsNearlyEqual(CameraRig->GetCameraHorizontalFovDegrees(), 60.0f));

		OutSpawnDistance = PlanetRadius
			+ static_cast<double>(Capsule->GetScaledCapsuleHalfHeight())
			+ SpawnClearance;
		return true;
	}

	FTransform MakeSurfaceTransform(
		const double AngleDegrees,
		const double RadialDistance)
	{
		const double AngleRadians = FMath::DegreesToRadians(AngleDegrees);
		const FVector SurfaceNormal(
			FMath::Sin(AngleRadians),
			0.0,
			FMath::Cos(AngleRadians));
		return FTransform(
			FRotator(-AngleDegrees, 0.0, 0.0),
			SurfaceNormal * RadialDistance,
			FVector::OneVector);
	}

	bool VerifyTransform(
		FAutomationTestBase& Test,
		const AActor& Actor,
		const FTransform& Expected,
		const TCHAR* Description)
	{
		bool bMatches = true;
		bMatches &= Test.TestTrue(
			FString::Printf(TEXT("%s location"), Description),
			Actor.GetActorLocation().Equals(Expected.GetLocation(), TransformTolerance));
		bMatches &= Test.TestTrue(
			FString::Printf(TEXT("%s rotation"), Description),
			Actor.GetActorRotation().Equals(Expected.Rotator(), RotationTolerance));
		bMatches &= Test.TestTrue(
			FString::Printf(TEXT("%s scale"), Description),
			Actor.GetActorScale3D().Equals(FVector::OneVector, TransformTolerance));
		return bMatches;
	}

	int32 CountActorsOfClass(const TArray<AActor*>& Actors, const UClass* Class)
	{
		int32 Count = 0;
		for (const AActor* Actor : Actors)
		{
			if (Actor && Actor->IsA(Class))
			{
				++Count;
			}
		}
		return Count;
	}

	void VerifySubclassActorsUseExactClass(
		FAutomationTestBase& Test,
		const TArray<AActor*>& Actors,
		const UClass* ExpectedClass,
		const TCHAR* Description)
	{
		for (const AActor* Actor : Actors)
		{
			if (Actor && Actor->IsA(ExpectedClass))
			{
				const UClass* ActualClass = Actor->GetClass();
				Test.TestEqual(
					FString::Printf(TEXT("%s uses exact native class: %s"), Description, *Actor->GetActorLabel()),
					ActualClass,
					ExpectedClass);
			}
		}
	}

	AActor* FindActorByLabel(
		const TMap<FString, AActor*>& ActorsByLabel,
		const FString& Label)
	{
		AActor* const* Found = ActorsByLabel.Find(Label);
		return Found ? *Found : nullptr;
	}

	bool VerifyPortalTrigger(
		FAutomationTestBase& Test,
		const AEverPlanetRegionPortal& Portal,
		const TCHAR* Description)
	{
		TArray<UBoxComponent*> BoxComponents;
		Portal.GetComponents<UBoxComponent>(BoxComponents);
		if (!Test.TestEqual(
				FString::Printf(TEXT("%s has exactly one box trigger"), Description),
				BoxComponents.Num(),
				1))
		{
			return false;
		}

		const UBoxComponent* Trigger = BoxComponents[0];
		Test.TestTrue(
			FString::Printf(TEXT("%s trigger relative location"), Description),
			Trigger->GetRelativeLocation().Equals(FVector(0.0, 0.0, 200.0), KINDA_SMALL_NUMBER));
		Test.TestTrue(
			FString::Printf(TEXT("%s trigger extent"), Description),
			Trigger->GetUnscaledBoxExtent().Equals(FVector(100.0, 300.0, 400.0), KINDA_SMALL_NUMBER));
		Test.TestEqual(
			FString::Printf(TEXT("%s trigger collision mode"), Description),
			Trigger->GetCollisionEnabled(),
			ECollisionEnabled::QueryOnly);
		Test.TestTrue(
			FString::Printf(TEXT("%s trigger overlap generation"), Description),
			Trigger->GetGenerateOverlapEvents());
		Test.TestEqual(
			FString::Printf(TEXT("%s trigger object channel"), Description),
			Trigger->GetCollisionObjectType(),
			ECC_WorldDynamic);

		for (int32 ChannelIndex = 0; ChannelIndex < static_cast<int32>(ECC_MAX); ++ChannelIndex)
		{
			const ECollisionChannel Channel = static_cast<ECollisionChannel>(ChannelIndex);
			const ECollisionResponse Expected = Channel == ECC_Pawn ? ECR_Overlap : ECR_Ignore;
			Test.TestEqual(
				FString::Printf(TEXT("%s collision response channel %d"), Description, ChannelIndex),
				Trigger->GetCollisionResponseToChannel(Channel),
				Expected);
		}
		return true;
	}

	bool VerifyMap(
		FAutomationTestBase& Test,
		const FMapContract& Contract,
		UClass* RegionalGameModeClass,
		const UEverPlanetWorldCatalog* Catalog,
		const double SpawnDistance)
	{
		Test.TestEqual(
			TEXT("Expected regional label contract cardinality"),
			Contract.ExpectedLabels.Num(),
			Contract.ActorCount);
		const FString MapFilename = FPackageName::LongPackageNameToFilename(
			Contract.MapPackage,
			FPackageName::GetMapPackageExtension());
		if (!FEditorFileUtils::LoadMap(MapFilename, false, false))
		{
			Test.AddError(FString::Printf(TEXT("Editor failed to load required generated map: %s"), Contract.MapPackage));
			return false;
		}

		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (!World)
		{
			Test.AddError(FString::Printf(TEXT("Editor World is unavailable after loading: %s"), Contract.MapPackage));
			return false;
		}
		Test.TestEqual(TEXT("Loaded exact regional map package"), World->GetOutermost()->GetName(), FString(Contract.MapPackage));
		Test.TestEqual(TEXT("Regional map is an Editor World"), World->WorldType.GetValue(), EWorldType::Editor);

		const AWorldSettings* WorldSettings = World->GetWorldSettings();
		if (!WorldSettings)
		{
			Test.AddError(FString::Printf(TEXT("WorldSettings is unavailable: %s"), Contract.MapPackage));
			return false;
		}
		Test.TestEqual(TEXT("Regional map GameMode override"), WorldSettings->DefaultGameMode.Get(), RegionalGameModeClass);
		Test.TestTrue(TEXT("Regional map KillZ"), FMath::IsNearlyEqual(static_cast<double>(WorldSettings->KillZ), KillZ, 0.1));

		UEditorActorSubsystem* ActorSubsystem =
			GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
		if (!ActorSubsystem)
		{
			Test.AddError(TEXT("EditorActorSubsystem is unavailable."));
			return false;
		}

		const TArray<AActor*> Actors = ActorSubsystem->GetAllLevelActors();
		Test.TestEqual(TEXT("Regional authored Actor cardinality"), Actors.Num(), Contract.ActorCount);
		TMap<FString, AActor*> ActorsByLabel;
		for (AActor* Actor : Actors)
		{
			if (!Actor)
			{
				Test.AddError(TEXT("Regional Actor list contains a null entry."));
				continue;
			}
			const FString Label = Actor->GetActorLabel();
			if (ActorsByLabel.Contains(Label))
			{
				Test.AddError(FString::Printf(TEXT("Regional Actor label is duplicated: %s"), *Label));
			}
			ActorsByLabel.Add(Label, Actor);
		}

		TSet<FString> ExpectedLabelSet;
		for (const FString& Label : Contract.ExpectedLabels)
		{
			ExpectedLabelSet.Add(Label);
			AActor* RequiredActor = FindActorByLabel(ActorsByLabel, Label);
			Test.TestNotNull(
				FString::Printf(TEXT("Required regional Actor exists: %s"), *Label),
				RequiredActor);
			if (RequiredActor && Label.StartsWith(TEXT("Env01_")))
			{
				Test.TestEqual(
					FString::Printf(TEXT("Environment Pass01 Actor uses exact StaticMeshActor class: %s"), *Label),
					RequiredActor->GetClass(),
					AStaticMeshActor::StaticClass());
				const AStaticMeshActor* StaticActor = Cast<AStaticMeshActor>(RequiredActor);
				const UStaticMeshComponent* Component =
					StaticActor ? StaticActor->GetStaticMeshComponent() : nullptr;
				if (Test.TestNotNull(
					FString::Printf(TEXT("Environment Pass01 StaticMeshComponent exists: %s"), *Label),
					Component))
				{
					Test.TestNotNull(
						FString::Printf(TEXT("Environment Pass01 StaticMesh exists: %s"), *Label),
						Component->GetStaticMesh().Get());
					Test.TestNotNull(
						FString::Printf(TEXT("Environment Pass01 material slot 0 exists: %s"), *Label),
						Component->GetMaterial(0));
					Test.TestEqual(
						FString::Printf(TEXT("Environment Pass01 mobility is Static: %s"), *Label),
						Component->GetMobility(),
						EComponentMobility::Static);
				}
			}
		}
		Test.TestEqual(
			TEXT("Expected regional labels are unique"),
			ExpectedLabelSet.Num(),
			Contract.ActorCount);
		for (const TPair<FString, AActor*>& Pair : ActorsByLabel)
		{
			Test.TestTrue(
				FString::Printf(TEXT("Regional Actor label belongs to the exact contract: %s"), *Pair.Key),
				ExpectedLabelSet.Contains(Pair.Key));
		}

		Test.TestEqual(TEXT("One PlanetBody"), CountActorsOfClass(Actors, APlanetBody::StaticClass()), 1);
		Test.TestEqual(TEXT("One RegionDescriptor"), CountActorsOfClass(Actors, AEverPlanetRegionDescriptor::StaticClass()), 1);
		Test.TestEqual(TEXT("Regional PlayerStart count"), CountActorsOfClass(Actors, AEverPlanetRegionPlayerStart::StaticClass()), Contract.RegionalStartCount);
		Test.TestEqual(TEXT("One RegionPortal"), CountActorsOfClass(Actors, AEverPlanetRegionPortal::StaticClass()), 1);
		Test.TestEqual(
			TEXT("Exact StaticMeshActor cardinality"),
			CountActorsOfClass(Actors, AStaticMeshActor::StaticClass()),
			Contract.StaticMeshActorCount);
		Test.TestEqual(TEXT("Two DirectionalLights"), CountActorsOfClass(Actors, ADirectionalLight::StaticClass()), 2);
		Test.TestEqual(TEXT("One SkyLight"), CountActorsOfClass(Actors, ASkyLight::StaticClass()), 1);
		Test.TestEqual(TEXT("One SkyAtmosphere"), CountActorsOfClass(Actors, ASkyAtmosphere::StaticClass()), 1);
		Test.TestEqual(TEXT("PlayerStart superclass count"), CountActorsOfClass(Actors, APlayerStart::StaticClass()), Contract.RegionalStartCount);
		int32 OrdinaryPlayerStarts = 0;
		for (const AActor* Actor : Actors)
		{
			if (Actor && Actor->GetClass() == APlayerStart::StaticClass())
			{
				++OrdinaryPlayerStarts;
			}
		}
		Test.TestEqual(TEXT("No ordinary PlayerStart"), OrdinaryPlayerStarts, 0);
		VerifySubclassActorsUseExactClass(Test, Actors, APlanetBody::StaticClass(), TEXT("PlanetBody"));
		VerifySubclassActorsUseExactClass(Test, Actors, AEverPlanetRegionDescriptor::StaticClass(), TEXT("RegionDescriptor"));
		VerifySubclassActorsUseExactClass(Test, Actors, AEverPlanetRegionPlayerStart::StaticClass(), TEXT("Regional PlayerStart"));
		VerifySubclassActorsUseExactClass(Test, Actors, AEverPlanetRegionPortal::StaticClass(), TEXT("RegionPortal"));
		VerifySubclassActorsUseExactClass(Test, Actors, AStaticMeshActor::StaticClass(), TEXT("StaticMeshActor"));
		VerifySubclassActorsUseExactClass(Test, Actors, ADirectionalLight::StaticClass(), TEXT("DirectionalLight"));
		VerifySubclassActorsUseExactClass(Test, Actors, ASkyLight::StaticClass(), TEXT("SkyLight"));
		VerifySubclassActorsUseExactClass(Test, Actors, ASkyAtmosphere::StaticClass(), TEXT("SkyAtmosphere"));

		const APlanetBody* Planet = Cast<APlanetBody>(FindActorByLabel(ActorsByLabel, TEXT("PlanetBody_0")));
		if (Test.TestNotNull(TEXT("PlanetBody_0 has the exact native class"), Planet))
		{
			Test.TestTrue(
				TEXT("Regional PlanetBody radius is 10000 cm"),
				FMath::IsNearlyEqual(static_cast<double>(Planet->GetSurfaceRadius()), PlanetRadius));
			VerifyTransform(Test, *Planet, FTransform::Identity, TEXT("PlanetBody_0"));
		}

		const AEverPlanetRegionDescriptor* Descriptor =
			Cast<AEverPlanetRegionDescriptor>(FindActorByLabel(ActorsByLabel, Contract.DescriptorLabel));
		if (Test.TestNotNull(TEXT("Exact RegionDescriptor Actor"), Descriptor))
		{
			Test.TestEqual(TEXT("Descriptor PlanetId"), Descriptor->GetRegionKey().PlanetId, PlanetId);
			Test.TestEqual(TEXT("Descriptor RegionId"), Descriptor->GetRegionKey().RegionId, Contract.RegionId);
			const UEverPlanetWorldCatalog* DescriptorCatalog = Descriptor->GetWorldCatalog();
			Test.TestEqual(TEXT("Descriptor Catalog reference"), DescriptorCatalog, Catalog);
			VerifyTransform(Test, *Descriptor, FTransform::Identity, Contract.DescriptorLabel);
		}

		TArray<const AEverPlanetRegionPlayerStart*> Starts;
		for (const FStartContract& StartContract : Contract.StartContracts)
		{
			const AEverPlanetRegionPlayerStart* Start = Cast<AEverPlanetRegionPlayerStart>(
				FindActorByLabel(ActorsByLabel, StartContract.Label));
			if (Test.TestNotNull(
					FString::Printf(TEXT("Exact regional start: %s"), StartContract.Label),
					Start))
			{
				Test.TestEqual(
					FString::Printf(TEXT("Stable anchor ID: %s"), StartContract.Label),
					Start->GetSpawnAnchorId(),
					StartContract.SpawnAnchorId);
				VerifyTransform(
					Test,
					*Start,
					MakeSurfaceTransform(StartContract.AngleDegrees, SpawnDistance),
					StartContract.Label);
				Starts.Add(Start);
			}
		}

		const AEverPlanetRegionPortal* Portal = Cast<AEverPlanetRegionPortal>(
			FindActorByLabel(ActorsByLabel, Contract.PortalLabel));
		if (Test.TestNotNull(TEXT("Exact RegionPortal Actor"), Portal))
		{
			Test.TestEqual(TEXT("Portal stable ID"), Portal->GetPortalId(), Contract.PortalId);
			Test.TestEqual(TEXT("Portal TravelLinkId"), Portal->GetTravelLinkId(), Contract.TravelLinkId);
			VerifyTransform(
				Test,
				*Portal,
				MakeSurfaceTransform(Contract.PortalAngleDegrees, PlanetRadius),
				Contract.PortalLabel);
			VerifyPortalTrigger(Test, *Portal, Contract.PortalLabel);

			const FVector PortalDirection = Portal->GetActorLocation().GetSafeNormal();
			for (const AEverPlanetRegionPlayerStart* Start : Starts)
			{
				const double Dot = FMath::Clamp(
					static_cast<double>(FVector::DotProduct(
						PortalDirection,
						Start->GetActorLocation().GetSafeNormal())),
					-1.0,
					1.0);
				const double ArcDistance = PlanetRadius * FMath::Acos(Dot);
				Test.TestTrue(
					FString::Printf(TEXT("%s is at least 600 cm from %s"), Contract.PortalLabel, *Start->GetActorLabel()),
					ArcDistance >= MinimumPortalAnchorArc);
			}
		}
		return true;
	}

	FMapContract MakeTownContract()
	{
		FMapContract Contract = {
			TownMapPackage,
			TownRegionId,
			86,
			77,
			2,
			TEXT("RegionDescriptor_Town"),
			TEXT("RegionPortal_Town_EastGate"),
			TownPortalId,
			TownToFieldLinkId,
			12.0,
			{
				TEXT("PlanetBody_0"), TEXT("DirectionalLight_Main"), TEXT("DirectionalLight_Fill"),
				TEXT("SkyLight_0"), TEXT("SkyAtmosphere_0"), TEXT("TownPlaza_0"),
				TEXT("TownHouse_Left"), TEXT("TownHouse_Right"), TEXT("TownGate_Left"),
				TEXT("TownGate_Right"), TEXT("RegionDescriptor_Town"),
				TEXT("RegionalStart_Town_Start"), TEXT("RegionalStart_Town_FromField"),
				TEXT("RegionPortal_Town_EastGate")
			},
			{
				{TEXT("RegionalStart_Town_Start"), TownStartAnchorId, 0.0},
				{TEXT("RegionalStart_Town_FromField"), TownFromFieldAnchorId, 8.0}
			}
		};

		for (int32 Index = 0; Index < 12; ++Index)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Town_Road_%02d"), Index));
		}
		const TCHAR* PlazaDirections[] = {TEXT("North"), TEXT("South"), TEXT("East"), TEXT("West")};
		for (const TCHAR* Direction : PlazaDirections)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Town_PlazaEdge_%s"), Direction));
		}
		const TCHAR* FountainParts[] = {TEXT("Base"), TEXT("Basin"), TEXT("Column"), TEXT("Water"), TEXT("Orb")};
		for (const TCHAR* Part : FountainParts)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Town_Fountain_%s"), Part));
		}
		const TCHAR* TownSides[] = {TEXT("Left"), TEXT("Right")};
		const TCHAR* HouseParts[] = {TEXT("Roof"), TEXT("Door"), TEXT("Chimney")};
		const TCHAR* CottageParts[] = {TEXT("Body"), TEXT("Roof"), TEXT("Door"), TEXT("Chimney")};
		for (const TCHAR* Side : TownSides)
		{
			for (const TCHAR* Part : HouseParts)
			{
				Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Town_House_%s_%s"), Side, Part));
			}
			for (const TCHAR* Part : CottageParts)
			{
				Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Town_Cottage_%s_%s"), Side, Part));
			}
		}
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Town_Tree_%02d_Trunk"), Index));
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Town_Tree_%02d_Crown"), Index));
		}
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Town_Lamp_%02d_Pole"), Index));
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Town_Lamp_%02d_Glow"), Index));
		}
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Town_Fence_%02d"), Index));
		}
		Contract.ExpectedLabels.Add(TEXT("Env01_Town_GateTop"));
		Contract.ExpectedLabels.Add(TEXT("Env01_Town_Sign_Post"));
		Contract.ExpectedLabels.Add(TEXT("Env01_Town_Sign_Board"));
		Contract.ExpectedLabels.Add(TEXT("Env01_Town_FlowerBed_Left"));
		Contract.ExpectedLabels.Add(TEXT("Env01_Town_FlowerBed_Right"));
		return Contract;
	}

	FMapContract MakeFieldContract()
	{
		FMapContract Contract = {
			FieldMapPackage,
			FieldRegionId,
			83,
			75,
			1,
			TEXT("RegionDescriptor_Field"),
			TEXT("RegionPortal_Field_WestGate"),
			FieldPortalId,
			FieldToTownLinkId,
			-12.0,
			{
				TEXT("PlanetBody_0"), TEXT("DirectionalLight_Main"), TEXT("DirectionalLight_Fill"),
				TEXT("SkyLight_0"), TEXT("SkyAtmosphere_0"), TEXT("FieldPath_0"),
				TEXT("FieldRock_0"), TEXT("FieldRock_1"), TEXT("FieldMarker_Left"),
				TEXT("FieldMarker_Right"), TEXT("RegionDescriptor_Field"),
				TEXT("RegionalStart_Field_FromTown"), TEXT("RegionPortal_Field_WestGate")
			},
			{
				{TEXT("RegionalStart_Field_FromTown"), FieldFromTownAnchorId, -8.0}
			}
		};

		for (int32 Index = 0; Index < 14; ++Index)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Field_Trail_%02d"), Index));
		}
		Contract.ExpectedLabels.Add(TEXT("Env01_Field_Pond"));
		for (int32 Index = 0; Index < 5; ++Index)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Field_Bridge_Deck_%02d"), Index));
		}
		Contract.ExpectedLabels.Add(TEXT("Env01_Field_Bridge_Rail_Left"));
		Contract.ExpectedLabels.Add(TEXT("Env01_Field_Bridge_Rail_Right"));
		for (int32 Index = 0; Index < 10; ++Index)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Field_Tree_%02d_Trunk"), Index));
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Field_Tree_%02d_Crown"), Index));
		}
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Field_Rock_%02d"), Index));
		}
		for (int32 Index = 0; Index < 6; ++Index)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Field_CliffCap_%02d"), Index));
		}
		for (int32 Index = 0; Index < 2; ++Index)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Field_FlowerPatch_%02d"), Index));
		}
		for (int32 Index = 0; Index < 6; ++Index)
		{
			Contract.ExpectedLabels.Add(FString::Printf(TEXT("Env01_Field_Ruin_Wall_%02d"), Index));
		}
		Contract.ExpectedLabels.Add(TEXT("Env01_Field_Ruin_Pillar_Left"));
		Contract.ExpectedLabels.Add(TEXT("Env01_Field_Ruin_Pillar_Right"));
		Contract.ExpectedLabels.Add(TEXT("Env01_Field_Sign_Post"));
		Contract.ExpectedLabels.Add(TEXT("Env01_Field_Sign_Board"));
		Contract.ExpectedLabels.Add(TEXT("Env01_Field_PortalGateTop"));
		Contract.ExpectedLabels.Add(TEXT("Env01_Field_FarLandmark"));
		return Contract;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFirstPlanetRegionalContentTest,
	"EverPlanet.M4.FirstPlanetRegionalContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFirstPlanetRegionalContentTest::RunTest(const FString& Parameters)
{
	using namespace EverPlanetFirstPlanetRegionalContentTest;

	bool bAllPackagesExist = true;
	bAllPackagesExist &= RequirePackage(*this, CatalogPackage, TEXT("World Catalog"));
	bAllPackagesExist &= RequirePackage(*this, ProductCharacterPackage, TEXT("Product Character"));
	bAllPackagesExist &= RequirePackage(*this, ProductGameModePackage, TEXT("Product GameMode"));
	bAllPackagesExist &= RequirePackage(*this, RegionalGameModePackage, TEXT("Regional GameMode"));
	bAllPackagesExist &= RequirePackage(*this, TownMapPackage, TEXT("Town map"));
	bAllPackagesExist &= RequirePackage(*this, FieldMapPackage, TEXT("Field map"));
	if (!bAllPackagesExist)
	{
		return false;
	}

	TStrongObjectPtr<UEverPlanetWorldCatalog> Catalog(
		LoadObject<UEverPlanetWorldCatalog>(nullptr, CatalogObjectPath));
	if (!Catalog.IsValid())
	{
		AddError(FString::Printf(TEXT("Generated World Catalog object failed to load: %s"), CatalogObjectPath));
		return false;
	}
	if (!VerifyCatalog(*this, *Catalog.Get()))
	{
		return false;
	}

	UClass* RegionalGameModeClass = nullptr;
	double SpawnDistance = 0.0;
	if (!VerifyClasses(
			*this,
			RegionalGameModeClass,
			SpawnDistance))
	{
		return false;
	}

	if (!VerifyMap(
		*this,
		MakeTownContract(),
		RegionalGameModeClass,
		Catalog.Get(),
		SpawnDistance))
	{
		return false;
	}
	return VerifyMap(
		*this,
		MakeFieldContract(),
		RegionalGameModeClass,
		Catalog.Get(),
		SpawnDistance);
}

#endif

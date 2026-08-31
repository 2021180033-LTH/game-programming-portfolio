// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "World/Data/EverPlanetWorldCatalog.h"

namespace EverPlanetWorldCatalogTests
{
	UEverPlanetWorldCatalog* MakeValidCatalog()
	{
		UEverPlanetWorldCatalog* Catalog = NewObject<UEverPlanetWorldCatalog>();
		Catalog->InitialAddress.PlanetId = TEXT("planet.first");
		Catalog->InitialAddress.RegionId = TEXT("region.first.town");
		Catalog->InitialAddress.SpawnAnchorId = TEXT("spawn.town.start");

		FEverPlanetPlanetDefinition Planet;
		Planet.PlanetId = TEXT("planet.first");
		Planet.DisplayName = FText::FromString(TEXT("First Planet"));
		Planet.DefaultRegionId = TEXT("region.first.town");
		Planet.RegionIds = {TEXT("region.first.town"), TEXT("region.first.field")};
		Catalog->Planets.Add(Planet);

		FEverPlanetRegionDefinition Town;
		Town.PlanetId = Planet.PlanetId;
		Town.RegionId = TEXT("region.first.town");
		Town.DisplayName = FText::FromString(TEXT("Town"));
		Town.Map = TSoftObjectPtr<UWorld>(
			FSoftObjectPath(TEXT("/Game/EverPlanet/Product/Planets/FirstPlanet/Maps/Lvl_FirstPlanet_Town.Lvl_FirstPlanet_Town")));
		Town.DefaultSpawnAnchorId = TEXT("spawn.town.start");
		Town.SpawnAnchorIds = {TEXT("spawn.town.start"), TEXT("spawn.town.from_field")};
		Catalog->Regions.Add(Town);

		FEverPlanetRegionDefinition Field;
		Field.PlanetId = Planet.PlanetId;
		Field.RegionId = TEXT("region.first.field");
		Field.DisplayName = FText::FromString(TEXT("Field"));
		Field.Map = TSoftObjectPtr<UWorld>(
			FSoftObjectPath(TEXT("/Game/EverPlanet/Product/Planets/FirstPlanet/Maps/Lvl_FirstPlanet_Field.Lvl_FirstPlanet_Field")));
		Field.DefaultSpawnAnchorId = TEXT("spawn.field.from_town");
		Field.SpawnAnchorIds = {TEXT("spawn.field.from_town")};
		Catalog->Regions.Add(Field);

		FEverPlanetTravelLinkDefinition ToField;
		ToField.LinkId = TEXT("link.first.town_to_field");
		ToField.Source.PlanetId = Planet.PlanetId;
		ToField.Source.RegionId = Town.RegionId;
		ToField.SourcePortalId = TEXT("portal.town.east_gate");
		ToField.Destination.PlanetId = Planet.PlanetId;
		ToField.Destination.RegionId = Field.RegionId;
		ToField.Destination.SpawnAnchorId = TEXT("spawn.field.from_town");
		ToField.bInitiallyUnlocked = true;
		ToField.ArrivalQuestFlag = TEXT("quest.first.field_visited");
		Catalog->TravelLinks.Add(ToField);

		FEverPlanetTravelLinkDefinition ToTown;
		ToTown.LinkId = TEXT("link.first.field_to_town");
		ToTown.Source.PlanetId = Planet.PlanetId;
		ToTown.Source.RegionId = Field.RegionId;
		ToTown.SourcePortalId = TEXT("portal.field.west_gate");
		ToTown.Destination.PlanetId = Planet.PlanetId;
		ToTown.Destination.RegionId = Town.RegionId;
		ToTown.Destination.SpawnAnchorId = TEXT("spawn.town.from_field");
		ToTown.bInitiallyUnlocked = false;
		ToTown.RequiredQuestFlag = TEXT("quest.first.field_visited");
		Catalog->TravelLinks.Add(ToTown);

		return Catalog;
	}

	void AddValidSecondPlanetRoute(UEverPlanetWorldCatalog& Catalog)
	{
		FEverPlanetPlanetDefinition SecondPlanet;
		SecondPlanet.PlanetId = TEXT("planet.second");
		SecondPlanet.DisplayName = FText::FromString(TEXT("Second Planet"));
		SecondPlanet.DefaultRegionId = TEXT("region.second.arrival");
		SecondPlanet.RegionIds = {TEXT("region.second.arrival")};
		Catalog.Planets.Add(SecondPlanet);

		FEverPlanetRegionDefinition Arrival;
		Arrival.PlanetId = SecondPlanet.PlanetId;
		Arrival.RegionId = SecondPlanet.DefaultRegionId;
		Arrival.DisplayName = FText::FromString(TEXT("Second Planet Arrival"));
		Arrival.Map = TSoftObjectPtr<UWorld>(
			FSoftObjectPath(TEXT("/Game/EverPlanet/Product/Planets/SecondPlanet/Maps/Lvl_SecondPlanet_Arrival.Lvl_SecondPlanet_Arrival")));
		Arrival.DefaultSpawnAnchorId = TEXT("spawn.second.from_first");
		Arrival.SpawnAnchorIds = {TEXT("spawn.second.from_first")};
		Catalog.Regions.Add(Arrival);

		FEverPlanetTravelLinkDefinition ToSecondPlanet;
		ToSecondPlanet.LinkId = TEXT("link.first.to_second");
		ToSecondPlanet.Scope = EEverPlanetTravelScope::InterPlanet;
		ToSecondPlanet.Source = FEverPlanetRegionKey(TEXT("planet.first"), TEXT("region.first.town"));
		ToSecondPlanet.SourcePortalId = TEXT("portal.town.star_gate");
		ToSecondPlanet.Destination.PlanetId = SecondPlanet.PlanetId;
		ToSecondPlanet.Destination.RegionId = Arrival.RegionId;
		ToSecondPlanet.Destination.SpawnAnchorId = Arrival.DefaultSpawnAnchorId;
		ToSecondPlanet.bInitiallyUnlocked = false;
		ToSecondPlanet.RequiredQuestFlag = TEXT("quest.first.star_gate_ready");
		Catalog.TravelLinks.Add(ToSecondPlanet);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEverPlanetWorldCatalogValidationTest,
	"EverPlanet.M4.WorldCatalogValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEverPlanetWorldCatalogValidationTest::RunTest(const FString& Parameters)
{
	using namespace EverPlanetWorldCatalogTests;

	TestTrue(TEXT("lowercase dotted stable ID is accepted"), UEverPlanetWorldCatalog::IsStableId(TEXT("region.first.town")));
	TestFalse(TEXT("uppercase ID is rejected"), UEverPlanetWorldCatalog::IsStableId(TEXT("Region.First.Town")));
	TestFalse(TEXT("whitespace ID is rejected"), UEverPlanetWorldCatalog::IsStableId(TEXT("region first town")));
	TestFalse(TEXT("leading separator is rejected"), UEverPlanetWorldCatalog::IsStableId(TEXT(".region.first.town")));

	EEverPlanetWorldFailure Failure = EEverPlanetWorldFailure::None;
	FString Error;

	UEverPlanetWorldCatalog* Catalog = MakeValidCatalog();
	TestTrue(TEXT("FirstPlanet Town/Field catalog validates"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("valid catalog reports no failure"), Failure, EEverPlanetWorldFailure::None);

	Catalog = MakeValidCatalog();
	const FEverPlanetTravelLinkDefinition DuplicateLink = Catalog->TravelLinks[0];
	Catalog->TravelLinks.Add(DuplicateLink);
	TestFalse(TEXT("duplicate LinkId fails closed"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("duplicate LinkId has exact failure"), Failure, EEverPlanetWorldFailure::DuplicateIdentifier);

	Catalog = MakeValidCatalog();
	Catalog->TravelLinks[0].Scope = EEverPlanetTravelScope::InterPlanet;
	TestFalse(TEXT("same-planet interplanet link fails closed"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("wrong link scope has exact failure"), Failure, EEverPlanetWorldFailure::InvalidCatalog);

	Catalog = MakeValidCatalog();
	Catalog->Regions[1].SpawnAnchorIds.Reset();
	TestFalse(TEXT("missing destination spawn fails closed"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("missing destination spawn has exact failure"), Failure, EEverPlanetWorldFailure::MissingSpawnAnchor);

	Catalog = MakeValidCatalog();
	Catalog->InitialAddress.RegionId = TEXT("region.first.missing");
	TestFalse(TEXT("unknown initial region fails closed"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("unknown initial region has exact failure"), Failure, EEverPlanetWorldFailure::UnknownRegion);

	Catalog = MakeValidCatalog();
	AddValidSecondPlanetRoute(*Catalog);
	Catalog->InitialAddress.PlanetId = TEXT("planet.second");
	TestFalse(TEXT("initial region must belong to the initial planet"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("foreign initial region has exact failure"), Failure, EEverPlanetWorldFailure::UnknownRegion);

	Catalog = MakeValidCatalog();
	Catalog->InitialAddress.SpawnAnchorId = TEXT("spawn.town.missing");
	TestFalse(TEXT("undeclared initial spawn fails closed"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("undeclared initial spawn has exact failure"), Failure, EEverPlanetWorldFailure::MissingSpawnAnchor);

	Catalog = MakeValidCatalog();
	Catalog->Regions[1].Map = Catalog->Regions[0].Map;
	TestFalse(TEXT("two regions cannot reuse one map"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("duplicate map has exact failure"), Failure, EEverPlanetWorldFailure::DestinationMapInvalid);

	Catalog = MakeValidCatalog();
	Catalog->TravelLinks[0].SourcePortalId = NAME_None;
	TestFalse(TEXT("source portal ID is mandatory"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("missing source portal has exact failure"), Failure, EEverPlanetWorldFailure::InvalidIdentifier);

	Catalog = MakeValidCatalog();
	FEverPlanetTravelLinkDefinition DuplicatePortalLink = Catalog->TravelLinks[0];
	DuplicatePortalLink.LinkId = TEXT("link.first.town_to_field.secondary");
	Catalog->TravelLinks.Add(DuplicatePortalLink);
	TestFalse(TEXT("source portal ID is unique inside its source region"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("duplicate source portal has exact failure"), Failure, EEverPlanetWorldFailure::DuplicateIdentifier);

	Catalog = MakeValidCatalog();
	Catalog->TravelLinks[1].SourcePortalId = Catalog->TravelLinks[0].SourcePortalId;
	TestTrue(TEXT("the same portal ID may exist in a different source region"), Catalog->ValidateCatalogDetailed(Failure, Error));

	Catalog = MakeValidCatalog();
	Catalog->TravelLinks[0].RequiredQuestFlag = TEXT("quest.first.field_open");
	TestFalse(TEXT("initially unlocked and quest-gated is an ambiguous policy"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("ambiguous unlock policy has exact failure"), Failure, EEverPlanetWorldFailure::InvalidCatalog);

	Catalog = MakeValidCatalog();
	Catalog->TravelLinks[1].RequiredQuestFlag = NAME_None;
	TestTrue(TEXT("an explicitly locked intra-planet link may wait for a runtime unlock"), Catalog->ValidateCatalogDetailed(Failure, Error));

	Catalog = MakeValidCatalog();
	AddValidSecondPlanetRoute(*Catalog);
	TestTrue(TEXT("locked quest-gated interplanet route validates"), Catalog->ValidateCatalogDetailed(Failure, Error));

	Catalog->TravelLinks.Last().RequiredQuestFlag = NAME_None;
	TestFalse(TEXT("interplanet route requires a quest gate"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("missing interplanet quest gate has exact failure"), Failure, EEverPlanetWorldFailure::InvalidCatalog);

	Catalog = MakeValidCatalog();
	AddValidSecondPlanetRoute(*Catalog);
	Catalog->TravelLinks.Last().bInitiallyUnlocked = true;
	TestFalse(TEXT("interplanet route cannot start unlocked"), Catalog->ValidateCatalogDetailed(Failure, Error));
	TestEqual(TEXT("initially unlocked interplanet route has exact failure"), Failure, EEverPlanetWorldFailure::InvalidCatalog);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEverPlanetWorldProgressSnapshotTest,
	"EverPlanet.M4.WorldProgressSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEverPlanetWorldProgressSnapshotTest::RunTest(const FString& Parameters)
{
	FEverPlanetWorldProgressSnapshot Snapshot;
	TestFalse(TEXT("NAME_None is not a completed quest flag"), Snapshot.HasQuestFlag(NAME_None));
	TestFalse(TEXT("unknown quest flag is not satisfied"), Snapshot.HasQuestFlag(TEXT("quest.first.field_visited")));

	Snapshot.CompletedQuestFlags.Add(TEXT("quest.first.field_visited"));
	Snapshot.UnlockedTravelLinkIds.Add(TEXT("link.first.field_to_town"));
	TestTrue(TEXT("stored quest flag is satisfied"), Snapshot.HasQuestFlag(TEXT("quest.first.field_visited")));
	TestTrue(
		TEXT("stored travel link is explicitly unlocked"),
		Snapshot.IsTravelLinkExplicitlyUnlocked(TEXT("link.first.field_to_town")));
	TestFalse(
		TEXT("other travel link remains locked"),
		Snapshot.IsTravelLinkExplicitlyUnlocked(TEXT("link.first.town_to_field")));

	FEverPlanetPendingTransition PendingTransition;
	TestEqual(
		TEXT("pending transition defaults to portal travel"),
		PendingTransition.Kind,
		EEverPlanetTransitionKind::PortalTravel);
	PendingTransition.Kind = EEverPlanetTransitionKind::StartupResume;
	TestEqual(
		TEXT("startup resume is an explicit transition kind"),
		PendingTransition.Kind,
		EEverPlanetTransitionKind::StartupResume);

	return true;
}

#endif

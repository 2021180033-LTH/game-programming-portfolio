// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/Data/EverPlanetWorldCatalog.h"

#include "Misc/PackageName.h"

namespace EverPlanetWorldCatalogValidation
{
	bool Fail(
		const EEverPlanetWorldFailure Failure,
		const FString& Message,
		EEverPlanetWorldFailure& OutFailure,
		FString& OutError)
	{
		OutFailure = Failure;
		OutError = Message;
		return false;
	}

	bool ContainsExactlyOnce(const TArray<FName>& Values, const FName Expected)
	{
		return Values.FilterByPredicate(
			[Expected](const FName Value)
			{
				return Value == Expected;
			}).Num() == 1;
	}
}

FPrimaryAssetId UEverPlanetWorldCatalog::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("EverPlanetWorldCatalog"), GetFName());
}

bool UEverPlanetWorldCatalog::IsStableId(const FName Identifier)
{
	if (Identifier.IsNone())
	{
		return false;
	}

	const FString Value = Identifier.ToString();
	if (Value.IsEmpty() || Value.Len() > 128)
	{
		return false;
	}

	for (const TCHAR Character : Value)
	{
		const bool bLowerAscii = Character >= TEXT('a') && Character <= TEXT('z');
		const bool bDigit = Character >= TEXT('0') && Character <= TEXT('9');
		const bool bSeparator = Character == TEXT('.')
			|| Character == TEXT('_')
			|| Character == TEXT('-');
		if (!bLowerAscii && !bDigit && !bSeparator)
		{
			return false;
		}
	}

	const TCHAR First = Value[0];
	const TCHAR Last = Value[Value.Len() - 1];
	const auto IsSeparator = [](const TCHAR Character)
	{
		return Character == TEXT('.') || Character == TEXT('_') || Character == TEXT('-');
	};
	return !IsSeparator(First) && !IsSeparator(Last);
}

const FEverPlanetPlanetDefinition* UEverPlanetWorldCatalog::FindPlanet(const FName PlanetId) const
{
	return Planets.FindByPredicate(
		[PlanetId](const FEverPlanetPlanetDefinition& Definition)
		{
			return Definition.PlanetId == PlanetId;
		});
}

const FEverPlanetRegionDefinition* UEverPlanetWorldCatalog::FindRegion(const FName RegionId) const
{
	return Regions.FindByPredicate(
		[RegionId](const FEverPlanetRegionDefinition& Definition)
		{
			return Definition.RegionId == RegionId;
		});
}

const FEverPlanetTravelLinkDefinition* UEverPlanetWorldCatalog::FindTravelLink(const FName LinkId) const
{
	return TravelLinks.FindByPredicate(
		[LinkId](const FEverPlanetTravelLinkDefinition& Definition)
		{
			return Definition.LinkId == LinkId;
		});
}

bool UEverPlanetWorldCatalog::ValidateCatalog(FString& OutError) const
{
	EEverPlanetWorldFailure Failure = EEverPlanetWorldFailure::None;
	return ValidateCatalogDetailed(Failure, OutError);
}

bool UEverPlanetWorldCatalog::ValidateCatalogDetailed(
	EEverPlanetWorldFailure& OutFailure,
	FString& OutError) const
{
	using namespace EverPlanetWorldCatalogValidation;

	OutFailure = EEverPlanetWorldFailure::None;
	OutError.Reset();
	if (Planets.IsEmpty())
	{
		return Fail(
			EEverPlanetWorldFailure::InvalidCatalog,
			TEXT("World catalog must contain at least one planet."),
			OutFailure,
			OutError);
	}
	if (Regions.IsEmpty())
	{
		return Fail(
			EEverPlanetWorldFailure::InvalidCatalog,
			TEXT("World catalog must contain at least one region."),
			OutFailure,
			OutError);
	}
	if (!InitialAddress.IsValid()
		|| !IsStableId(InitialAddress.PlanetId)
		|| !IsStableId(InitialAddress.RegionId)
		|| !IsStableId(InitialAddress.SpawnAnchorId))
	{
		return Fail(
			EEverPlanetWorldFailure::InvalidIdentifier,
			TEXT("World catalog InitialAddress is invalid."),
			OutFailure,
			OutError);
	}

	TSet<FName> PlanetIds;
	for (const FEverPlanetPlanetDefinition& Planet : Planets)
	{
		if (!IsStableId(Planet.PlanetId) || !IsStableId(Planet.DefaultRegionId))
		{
			return Fail(
				EEverPlanetWorldFailure::InvalidIdentifier,
				FString::Printf(TEXT("Planet has an invalid stable ID: %s"), *Planet.PlanetId.ToString()),
				OutFailure,
				OutError);
		}
		if (PlanetIds.Contains(Planet.PlanetId))
		{
			return Fail(
				EEverPlanetWorldFailure::DuplicateIdentifier,
				FString::Printf(TEXT("Duplicate PlanetId: %s"), *Planet.PlanetId.ToString()),
				OutFailure,
				OutError);
		}
		PlanetIds.Add(Planet.PlanetId);

		TSet<FName> OwnedRegionIds;
		for (const FName RegionId : Planet.RegionIds)
		{
			if (!IsStableId(RegionId))
			{
				return Fail(
					EEverPlanetWorldFailure::InvalidIdentifier,
					FString::Printf(TEXT("Planet %s contains an invalid RegionId."), *Planet.PlanetId.ToString()),
					OutFailure,
					OutError);
			}
			if (OwnedRegionIds.Contains(RegionId))
			{
				return Fail(
					EEverPlanetWorldFailure::DuplicateIdentifier,
					FString::Printf(
						TEXT("Planet %s lists RegionId %s more than once."),
						*Planet.PlanetId.ToString(),
						*RegionId.ToString()),
					OutFailure,
					OutError);
			}
			OwnedRegionIds.Add(RegionId);

			const FEverPlanetRegionDefinition* OwnedRegion = FindRegion(RegionId);
			if (!OwnedRegion || OwnedRegion->PlanetId != Planet.PlanetId)
			{
				return Fail(
					EEverPlanetWorldFailure::UnknownRegion,
					FString::Printf(
						TEXT("Planet %s lists unknown or foreign RegionId %s."),
						*Planet.PlanetId.ToString(),
						*RegionId.ToString()),
					OutFailure,
					OutError);
			}
		}
		if (!OwnedRegionIds.Contains(Planet.DefaultRegionId))
		{
			return Fail(
				EEverPlanetWorldFailure::InvalidCatalog,
				FString::Printf(
					TEXT("Planet %s default RegionId %s is not in its RegionIds."),
					*Planet.PlanetId.ToString(),
					*Planet.DefaultRegionId.ToString()),
				OutFailure,
				OutError);
		}
	}
	if (!FindPlanet(InitialAddress.PlanetId))
	{
		return Fail(
			EEverPlanetWorldFailure::UnknownPlanet,
			FString::Printf(
				TEXT("InitialAddress PlanetId %s does not exist."),
				*InitialAddress.PlanetId.ToString()),
			OutFailure,
			OutError);
	}

	TSet<FName> RegionIds;
	TSet<FName> RegionMapPackageNames;
	for (const FEverPlanetRegionDefinition& Region : Regions)
	{
		if (!IsStableId(Region.PlanetId)
			|| !IsStableId(Region.RegionId)
			|| !IsStableId(Region.DefaultSpawnAnchorId))
		{
			return Fail(
				EEverPlanetWorldFailure::InvalidIdentifier,
				FString::Printf(TEXT("Region has an invalid stable ID: %s"), *Region.RegionId.ToString()),
				OutFailure,
				OutError);
		}
		if (RegionIds.Contains(Region.RegionId))
		{
			return Fail(
				EEverPlanetWorldFailure::DuplicateIdentifier,
				FString::Printf(TEXT("Duplicate RegionId: %s"), *Region.RegionId.ToString()),
				OutFailure,
				OutError);
		}
		RegionIds.Add(Region.RegionId);
		// Validate path structure only: transient automation catalogs intentionally reference
		// future /Game maps, while runtime loading remains the authority for disk existence.
		const FSoftObjectPath MapPath = Region.Map.ToSoftObjectPath();
		const FString MapPackageName = MapPath.GetLongPackageName();
		const FName MapPackageId(*MapPackageName);
		if (Region.Map.IsNull()
			|| !MapPath.IsValid()
			|| MapPackageName.IsEmpty()
			|| !FPackageName::IsValidLongPackageName(MapPackageName)
			|| MapPath.GetAssetName() != FPackageName::GetShortName(MapPackageName))
		{
			return Fail(
				EEverPlanetWorldFailure::DestinationMapInvalid,
				FString::Printf(TEXT("Region %s has an invalid map soft reference."), *Region.RegionId.ToString()),
				OutFailure,
				OutError);
		}
		if (RegionMapPackageNames.Contains(MapPackageId))
		{
			return Fail(
				EEverPlanetWorldFailure::DestinationMapInvalid,
				FString::Printf(
					TEXT("Region %s reuses map package %s."),
					*Region.RegionId.ToString(),
					*MapPackageName),
				OutFailure,
				OutError);
		}
		RegionMapPackageNames.Add(MapPackageId);

		const FEverPlanetPlanetDefinition* Owner = FindPlanet(Region.PlanetId);
		if (!Owner)
		{
			return Fail(
				EEverPlanetWorldFailure::UnknownPlanet,
				FString::Printf(
					TEXT("Region %s references unknown PlanetId %s."),
					*Region.RegionId.ToString(),
					*Region.PlanetId.ToString()),
				OutFailure,
				OutError);
		}
		if (!ContainsExactlyOnce(Owner->RegionIds, Region.RegionId))
		{
			return Fail(
				EEverPlanetWorldFailure::InvalidCatalog,
				FString::Printf(
					TEXT("Region %s must be listed exactly once by Planet %s."),
					*Region.RegionId.ToString(),
					*Region.PlanetId.ToString()),
				OutFailure,
				OutError);
		}

		TSet<FName> SpawnAnchorIds;
		for (const FName SpawnAnchorId : Region.SpawnAnchorIds)
		{
			if (!IsStableId(SpawnAnchorId))
			{
				return Fail(
					EEverPlanetWorldFailure::InvalidIdentifier,
					FString::Printf(TEXT("Region %s has an invalid spawn anchor ID."), *Region.RegionId.ToString()),
					OutFailure,
					OutError);
			}
			if (SpawnAnchorIds.Contains(SpawnAnchorId))
			{
				return Fail(
					EEverPlanetWorldFailure::DuplicateSpawnAnchor,
					FString::Printf(
						TEXT("Region %s lists spawn anchor %s more than once."),
						*Region.RegionId.ToString(),
						*SpawnAnchorId.ToString()),
					OutFailure,
					OutError);
			}
			SpawnAnchorIds.Add(SpawnAnchorId);
		}
		if (!SpawnAnchorIds.Contains(Region.DefaultSpawnAnchorId))
		{
			return Fail(
				EEverPlanetWorldFailure::MissingSpawnAnchor,
				FString::Printf(
					TEXT("Region %s default spawn anchor %s is not declared."),
					*Region.RegionId.ToString(),
					*Region.DefaultSpawnAnchorId.ToString()),
				OutFailure,
				OutError);
		}
	}

	const FEverPlanetRegionDefinition* InitialRegion = FindRegion(InitialAddress.RegionId);
	if (!InitialRegion || InitialRegion->PlanetId != InitialAddress.PlanetId)
	{
		return Fail(
			EEverPlanetWorldFailure::UnknownRegion,
			FString::Printf(
				TEXT("InitialAddress region %s does not belong to planet %s."),
				*InitialAddress.RegionId.ToString(),
				*InitialAddress.PlanetId.ToString()),
			OutFailure,
			OutError);
	}
	if (!InitialRegion->SpawnAnchorIds.Contains(InitialAddress.SpawnAnchorId))
	{
		return Fail(
			EEverPlanetWorldFailure::MissingSpawnAnchor,
			FString::Printf(
				TEXT("InitialAddress targets undeclared spawn anchor %s in region %s."),
				*InitialAddress.SpawnAnchorId.ToString(),
				*InitialAddress.RegionId.ToString()),
			OutFailure,
			OutError);
	}

	TSet<FName> LinkIds;
	TSet<FString> SourcePortalKeys;
	for (const FEverPlanetTravelLinkDefinition& Link : TravelLinks)
	{
		if (!IsStableId(Link.LinkId)
			|| !Link.Source.IsValid()
			|| !IsStableId(Link.Source.PlanetId)
			|| !IsStableId(Link.Source.RegionId)
			|| !IsStableId(Link.SourcePortalId)
			|| !Link.Destination.IsValid()
			|| !IsStableId(Link.Destination.PlanetId)
			|| !IsStableId(Link.Destination.RegionId)
			|| !IsStableId(Link.Destination.SpawnAnchorId)
			|| (!Link.RequiredQuestFlag.IsNone() && !IsStableId(Link.RequiredQuestFlag))
			|| (!Link.ArrivalQuestFlag.IsNone() && !IsStableId(Link.ArrivalQuestFlag)))
		{
			return Fail(
				EEverPlanetWorldFailure::InvalidIdentifier,
				FString::Printf(TEXT("Travel link has an invalid stable ID: %s"), *Link.LinkId.ToString()),
				OutFailure,
				OutError);
		}
		if (LinkIds.Contains(Link.LinkId))
		{
			return Fail(
				EEverPlanetWorldFailure::DuplicateIdentifier,
				FString::Printf(TEXT("Duplicate LinkId: %s"), *Link.LinkId.ToString()),
				OutFailure,
				OutError);
		}
		LinkIds.Add(Link.LinkId);

		const FString SourcePortalKey = FString::Printf(
			TEXT("%s@%s"),
			*Link.Source.ToString(),
			*Link.SourcePortalId.ToString());
		if (SourcePortalKeys.Contains(SourcePortalKey))
		{
			return Fail(
				EEverPlanetWorldFailure::DuplicateIdentifier,
				FString::Printf(
					TEXT("Source portal %s is assigned more than once in region %s."),
					*Link.SourcePortalId.ToString(),
					*Link.Source.ToString()),
				OutFailure,
				OutError);
		}
		SourcePortalKeys.Add(SourcePortalKey);

		const FEverPlanetRegionDefinition* SourceRegion = FindRegion(Link.Source.RegionId);
		const FEverPlanetRegionDefinition* DestinationRegion = FindRegion(Link.Destination.RegionId);
		if (!SourceRegion
			|| SourceRegion->PlanetId != Link.Source.PlanetId
			|| !DestinationRegion
			|| DestinationRegion->PlanetId != Link.Destination.PlanetId)
		{
			return Fail(
				EEverPlanetWorldFailure::UnknownRegion,
				FString::Printf(TEXT("Travel link %s references an unknown or mismatched region."), *Link.LinkId.ToString()),
				OutFailure,
				OutError);
		}
		if (Link.Source == Link.Destination.GetRegionKey())
		{
			return Fail(
				EEverPlanetWorldFailure::InvalidCatalog,
				FString::Printf(TEXT("Travel link %s points to its own source region."), *Link.LinkId.ToString()),
				OutFailure,
				OutError);
		}
		const bool bSamePlanet = Link.Source.PlanetId == Link.Destination.PlanetId;
		if ((Link.Scope == EEverPlanetTravelScope::IntraPlanet && !bSamePlanet)
			|| (Link.Scope == EEverPlanetTravelScope::InterPlanet && bSamePlanet))
		{
			return Fail(
				EEverPlanetWorldFailure::InvalidCatalog,
				FString::Printf(TEXT("Travel link %s scope does not match its planets."), *Link.LinkId.ToString()),
				OutFailure,
				OutError);
		}
		if (Link.Scope == EEverPlanetTravelScope::InterPlanet
			&& (Link.bInitiallyUnlocked || Link.RequiredQuestFlag.IsNone()))
		{
			return Fail(
				EEverPlanetWorldFailure::InvalidCatalog,
				FString::Printf(
					TEXT("Interplanet travel link %s must start locked and require a quest flag."),
					*Link.LinkId.ToString()),
				OutFailure,
				OutError);
		}
		if (Link.bInitiallyUnlocked && !Link.RequiredQuestFlag.IsNone())
		{
			return Fail(
				EEverPlanetWorldFailure::InvalidCatalog,
				FString::Printf(
					TEXT("Travel link %s cannot be both initially unlocked and quest-gated."),
					*Link.LinkId.ToString()),
				OutFailure,
				OutError);
		}
		if (!DestinationRegion->SpawnAnchorIds.Contains(Link.Destination.SpawnAnchorId))
		{
			return Fail(
				EEverPlanetWorldFailure::MissingSpawnAnchor,
				FString::Printf(
					TEXT("Travel link %s targets undeclared spawn anchor %s."),
					*Link.LinkId.ToString(),
					*Link.Destination.SpawnAnchorId.ToString()),
				OutFailure,
				OutError);
		}
	}

	return true;
}

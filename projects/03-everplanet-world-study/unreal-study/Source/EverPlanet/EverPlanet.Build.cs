// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EverPlanet : ModuleRules
{
	public EverPlanet(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"EverPlanet",
			"EverPlanet/Variant_Platforming",
			"EverPlanet/Variant_Platforming/Animation",
			"EverPlanet/Variant_Combat",
			"EverPlanet/Variant_Combat/AI",
			"EverPlanet/Variant_Combat/Animation",
			"EverPlanet/Variant_Combat/Gameplay",
			"EverPlanet/Variant_Combat/Interfaces",
			"EverPlanet/Variant_Combat/UI",
			"EverPlanet/Variant_SideScrolling",
			"EverPlanet/Variant_SideScrolling/AI",
			"EverPlanet/Variant_SideScrolling/Gameplay",
			"EverPlanet/Variant_SideScrolling/Interfaces",
			"EverPlanet/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

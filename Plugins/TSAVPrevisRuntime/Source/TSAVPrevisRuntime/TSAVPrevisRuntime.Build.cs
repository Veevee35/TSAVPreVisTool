// Copyright TSAV. All Rights Reserved.

using UnrealBuildTool;

public class TSAVPrevisRuntime : ModuleRules
{
	public TSAVPrevisRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		if (Target.bBuildEditor) PrivateDependencyModuleNames.Add("UnrealEd");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"CinematicCamera",
				"Engine",
				"EnhancedInput",
				"InputCore",
				"Json",
				"JsonUtilities",
				"Networking",
				"Sockets",
				"Slate",
				"SlateCore",
				"TSAVLEDTools",
				"UMG",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"Projects",
				"DMXRuntime",
				"DMXProtocol",
				"DMXGDTF",
				"DMXZip",
				"XmlParser",
				"MediaAssets",
				"ImageCore",
				"ProceduralMeshComponent",
			}
		);
	}
}

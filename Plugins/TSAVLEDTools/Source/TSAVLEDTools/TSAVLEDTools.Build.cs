// Copyright TSAV. All Rights Reserved.

using UnrealBuildTool;

public class TSAVLEDTools : ModuleRules
{
	public TSAVLEDTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"AssetRegistry",
				"DMXProtocol",
				"DMXRuntime",
				"Engine",
				"MediaAssets",
				"Json",
				"ProceduralMeshComponent",
			}
		);

		PrivateDependencyModuleNames.Add("MediaIOCore");
	}
}

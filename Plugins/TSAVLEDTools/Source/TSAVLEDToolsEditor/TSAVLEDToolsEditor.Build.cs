// Copyright TSAV. All Rights Reserved.

using UnrealBuildTool;

public class TSAVLEDToolsEditor : ModuleRules
{
	public TSAVLEDToolsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"AssetTools",
				"Core",
				"CoreUObject",
				"DesktopPlatform",
				"DMXGDTF",
				"DMXProtocol",
				"DMXRuntime",
				"DMXZip",
				"Engine",
				"InputCore",
				"LevelEditor",
				"MediaAssets",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"TSAVLEDTools",
				"UnrealEd",
			}
		);
	}
}

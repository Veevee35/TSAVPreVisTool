// Copyright TSAV. All Rights Reserved.

using UnrealBuildTool;

public class TSAVPrevisRuntimeEditor : ModuleRules
{
	public TSAVPrevisRuntimeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"ContentBrowser",
				"AssetRegistry",
				"DMXProtocol",
				"DMXRuntime",
				"Engine",
				"InputCore",
				"JsonUtilities",
				"Json",
				"UMG",
				"RenderCore",
				"LevelEditor",
				"MediaAssets",
				"PropertyEditor",
				"Projects",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"TSAVLEDTools",
				"TSAVPrevisRuntime",
				"UnrealEd",
			}
		);
	}
}

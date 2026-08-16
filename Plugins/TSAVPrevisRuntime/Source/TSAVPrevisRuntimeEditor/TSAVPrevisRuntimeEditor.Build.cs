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
				"Engine",
				"InputCore",
				"LevelEditor",
				"MediaAssets",
				"PropertyEditor",
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

// Copyright TSAV. All Rights Reserved.

using UnrealBuildTool;

public class TSAVPrevisRuntime : ModuleRules
{
	public TSAVPrevisRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"EnhancedInput",
				"InputCore",
				"TSAVLEDTools",
				"UMG",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
			}
		);
	}
}

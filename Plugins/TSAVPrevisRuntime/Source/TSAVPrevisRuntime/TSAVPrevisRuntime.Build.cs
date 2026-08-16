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
				"CinematicCamera",
				"Engine",
				"EnhancedInput",
				"InputCore",
				"Json",
				"JsonUtilities",
				"Networking",
				"Sockets",
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

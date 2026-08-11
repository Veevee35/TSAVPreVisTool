// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TSAVPrevisTools : ModuleRules
{
	public TSAVPrevisTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ToolsetRegistry",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"DMXProtocol",
				"Engine",
				"Json",
				"UnrealEd",
			}
		);
	}
}

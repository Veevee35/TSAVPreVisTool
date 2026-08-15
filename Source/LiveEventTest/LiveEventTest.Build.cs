// Copyright TSAV. All Rights Reserved.

using UnrealBuildTool;

public class LiveEventTest : ModuleRules
{
	public LiveEventTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core" });
	}
}

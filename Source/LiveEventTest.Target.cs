// Copyright TSAV. All Rights Reserved.

using UnrealBuildTool;

public class LiveEventTestTarget : TargetRules
{
	public LiveEventTestTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LiveEventTest");
		// The descriptor owns the installed-engine-compatible runtime allowlist.
		// TSAV plugin dependencies bring in Enhanced Input, DMX, NDI and procedural meshes.
	}
}

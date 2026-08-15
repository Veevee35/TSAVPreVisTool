// Copyright TSAV. All Rights Reserved.

using UnrealBuildTool;

public class LiveEventTestEditorTarget : TargetRules
{
	public LiveEventTestEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LiveEventTest");

		// Development-only integrations stay available to Codex/MCP and content
		// authors without becoming dependencies of the standalone application.
		EnablePlugins.AddRange(new[]
		{
			"TSAVPrevisTools", "OpenXR", "VirtualScouting", "PythonScriptPlugin",
			"HDRIBackdrop", "SunPosition", "SequencerScripting", "Takes",
			"VirtualProductionUtilities", "LiveLink", "Composite", "MediaFrameworkUtilities",
			"MediaIOFramework", "RemoteControl", "nDisplay", "AjaMedia", "BlackmagicMedia",
			"AppleProResMedia", "HAPMedia", "PixelStreaming", "VariantManager", "DMXProtocol",
			"MovieRenderPipeline", "DatasmithCADImporter", "DatasmithImporter", "DatasmithInterchange",
			"DatasmithMVR", "DatasmithFBXImporter", "DatasmithRuntime", "DaySequence",
			"DMXControlConsole", "DMXDisplayCluster", "DMXEngine", "DMXFixtures", "DMXPixelMapping",
			"LiveLinkControlRig", "LiveLinkHub", "LiveLinkOverNDisplay", "LiveLinkCamera",
			"MetaHuman", "MetaHumanCalibrationProcessing", "MetaHumanCoreTech", "MetaHumanLiveLink",
			"MetaHumanCharacter", "MetaHumanCalibrationDiagnostics", "MetasoundExperimental",
			"MIDIDevice", "NDIMedia", "PanoramicCapture", "RemoteControlProtocolDMX",
			"RemoteControlProtocolMIDI", "RemoteControlProtocolOSC", "RemoteControlWebInterface",
			"RemoteDatabaseSupport", "WinDualShock", "ModelContextProtocol", "MCPClientToolset", "AllToolsets",
		});
	}
}

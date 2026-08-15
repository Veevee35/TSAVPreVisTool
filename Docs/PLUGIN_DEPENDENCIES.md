# Plugin dependencies

## Project plugin graph

```text
TSAVPrevisRuntime (Runtime)
  +-- TSAVLEDTools (Runtime)
  |     +-- DMXProtocol
  |     +-- DMXRuntime
  |     +-- MediaAssets
  |     +-- ProceduralMeshComponent
  |
  +-- EnhancedInput
  +-- Engine / Core / CoreUObject / InputCore
  +-- UMG / Slate / SlateCore

TSAVLEDToolsEditor (Editor)
  +-- TSAVLEDTools
  +-- AssetRegistry / AssetTools / DesktopPlatform
  +-- DMXGDTF / DMXZip / DMXProtocol / DMXRuntime
  +-- LevelEditor / PropertyEditor / ToolMenus / UnrealEd

TSAVPrevisTools (Editor)
  +-- ToolsetRegistry
  +-- DMXProtocol
  +-- UnrealEd
```

The runtime module audit contains no reference to `UnrealEd`, `AssetTools`, `LevelEditor`, `PropertyEditor`, `ToolMenus`, `TSAVLEDToolsEditor`, or `TSAVPrevisTools`. The Win64 non-editor target compiles successfully.

## Project descriptors

| Plugin | Modules | Descriptor dependencies |
|---|---|---|
| `TSAVPrevisRuntime` | `TSAVPrevisRuntime: Runtime` | `EnhancedInput`, `TSAVLEDTools` |
| `TSAVLEDTools` | `TSAVLEDTools: Runtime`, `TSAVLEDToolsEditor: Editor` | `DMXEngine`, `NDIMedia`, `ProceduralMeshComponent` |
| `TSAVPrevisTools` | `TSAVPrevisTools: Editor` | `ToolsetRegistry`, `DMXProtocol`, `DMXEngine`, `NDIMedia` |

`TSAVPrevisTools` is also marked `EditorOnly`, `NoRedist`, and Editor target-only. It is not a packaged application dependency.

## Enabled project-plugin inventory

The `.uproject` now enables 61 entries, including the new runtime plugin. Grouped by capability:

- TSAV: `TSAVPrevisRuntime`, `TSAVLEDTools`.
- MCP/development: `ModelContextProtocol`, `MCPClientToolset`, `AllToolsets`, `PythonScriptPlugin`.
- Input/XR: `EnhancedInput` (engine default), `OpenXR`, `VirtualScouting`, `WinDualShock`.
- Media/video IO: `Composite`, `MediaFrameworkUtilities`, `MediaIOFramework`, `AjaMedia`, `BlackmagicMedia`, `AppleProResMedia`, `HAPMedia`, `NDIMedia`, `PixelStreaming`.
- DMX/control: `DMXProtocol`, `DMXControlConsole`, `DMXDisplayCluster`, `DMXEngine`, `DMXFixtures`, `DMXPixelMapping`, `RemoteControl`, `RemoteControlProtocolDMX`, `RemoteControlProtocolMIDI`, `RemoteControlProtocolOSC`, `RemoteControlWebInterface`, `MIDIDevice`.
- Datasmith/import: `DatasmithCADImporter`, `DatasmithImporter`, `DatasmithInterchange`, `DatasmithMVR`, `DatasmithFBXImporter`, `DatasmithRuntime`.
- Virtual production/camera: `HDRIBackdrop`, `SunPosition`, `SequencerScripting`, `Takes`, `VirtualProductionUtilities`, `LiveLink`, `LiveLinkControlRig`, `LiveLinkHub`, `LiveLinkOverNDisplay`, `LiveLinkCamera`, `nDisplay`, `MovieRenderPipeline`, `DaySequence`, `VariantManager`, `PanoramicCapture`.
- MetaHuman/Apple capture: `MetaHuman`, `MetaHumanCalibrationProcessing`, `MetaHumanCoreTech`, `MetaHumanLiveLink`, `MetaHumanCharacter`, `MetaHumanCalibrationDiagnostics`, `AppleARKit`, `AppleARKitFaceSupport`.
- Other infrastructure: `MetasoundExperimental`, `RemoteDatabaseSupport`.

Many of these plugins are editor-facing or future integrations. The Phase 1 one-map Development package cooks 1,227 packages and archives to approximately 2.2 GiB, confirming that a shipping allowlist is a concrete packaging requirement rather than only a later optimization. That review should reduce startup work, shader permutations, and package size while retaining the media/DMX integrations selected for a release.

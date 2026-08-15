# Runtime versus editor classification

## Runtime and shipped

### `TSAVLEDTools`

- `ATSAVMediaSurfaceActor`: media playback, canvas mapping, materials, and runtime surface lifecycle.
- `ATSAVLEDPanel`: single cabinet geometry and panel-definition integration.
- `ATSAVLEDWall`: parametric wall geometry, curves, irregular cells, topology, and canvas resolution.
- `ATSAVDMXFixture`: articulated mesh hierarchy, fixture patch, DMX response, motion, color, dimmer, zoom, and beam.
- `UTSAVLEDPanelDefinition`: runtime-safe cabinet data asset.

### `TSAVPrevisRuntime`

- `ATSAVAppGameMode`, `ATSAVPlayerController`, `ATSAVEditPawn`.
- `UTSAVMainWidget` runtime UMG shell.
- Project, mode, and selection subsystems.
- Scene object identity/component, selection interface, and smoke actor.

No runtime class includes an editor module dependency. `WITH_EDITOR` is used only where the existing LED actor calls editor construction scripts.

## Editor-only and not shipped

### `TSAVLEDToolsEditor`

- `STSAVLEDWallBuilder`.
- `STSAVDMXFixtureBuilder`.
- Editor menu/tab registration.
- GDTF archive extraction and editor asset import.
- Dependencies on `AssetTools`, `DesktopPlatform`, `LevelEditor`, `PropertyEditor`, `ToolMenus`, and `UnrealEd`.

These tools remain the development-time authoring path for reusable panel and fixture assets. Their runtime replacement is UMG over the existing runtime actors, not an attempt to ship the Slate windows.

### `TSAVPrevisTools`

- `UTSAVPrevisToolset` and `GetDMXStatus()`.
- Toolset Registry registration.
- MCP/Codex diagnostics.

The descriptor enforces `EditorOnly`, `NoRedist`, and an Editor target allowlist.

## Blueprint/content-only

- `/Game/Showcase/TSAV_Feature_Showcase` uses engine DMX fixture Blueprints such as `BP_MovingHead`, `BP_WashLED`, and `BP_StaticStrobe`.
- Existing media sources, players, playlists, materials, GDTF assets, imported models, and levels are content-only.
- `/Game/TSAV/App/L_TSAV_App` is the only new binary asset in Phase 1 and was created, inspected, configured, and saved through Unreal MCP.

## Migration rule

For each feature:

1. Keep or create its actual behavior in a Runtime module.
2. Expose it to the packaged operator through UMG.
3. Keep editor import/build conveniences in an Editor module.
4. Add a small number of structured MCP diagnostics or deterministic smoke operations where they improve development verification.

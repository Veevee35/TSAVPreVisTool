# Test baseline

Baseline date: 2026-08-15. Engine: Unreal Engine 5.8.1, changelist 56057345. Platform toolchain: Visual Studio 2022 14.44.35228 and Windows SDK 10.0.26100.0.

## Build matrix

| Check | Result | Evidence |
|---|---|---|
| Editor target before runtime work | Pass | `LiveEventTestEditor Win64 Development` |
| `TSAVPrevisRuntime` editor compile | Pass | Runtime module, subsystems, input, UI, and map wiring compiled |
| Non-editor application target | Pass | `LiveEventTest Win64 Development`; linked `Binaries/Win64/LiveEventTest.exe` |
| One-map Win64 Development cook/package | Pass | `BuildCookRun` for `/Game/TSAV/App/L_TSAV_App`; 1,227 packages cooked and archived to `Saved/Packages/Win64Development` |
| Packaged executable launch | Pass | `-NullRHI -Unattended -ExecCmds=quit`; exit code 0 after loading `ATSAVAppGameMode` and `L_TSAV_App` |
| Packaged application chrome | Pass | Direct 1440x900 window capture shows the UMG shell and no mobile touch overlay (`Saved/TSAVWindowDiagnostic.png`) |
| Runtime click selection | Pass | LMB selected `Demo Object 1`; outliner and inspector updated with its GUID and transform (`Saved/TSAVSelectionDiagnostic.png`) |
| Runtime viewport navigation | Pass | Focused-viewport W input moved the edit pawn through the smoke scene (`Saved/TSAVNavigationDiagnostic2.png`) |

## Existing LED validation

The latest repository log contains six successful Unreal Python assertions from 2026-08-14:

- `CODEX_LED_DUAL_ANGLE_S_CURVE_SUCCESS`
- `CODEX_LED_INTERNAL_RADIUS_MIGRATION_SUCCESS`
- `CODEX_LED_INTERNAL_CURVE_SEAM_SUCCESS`
- `CODEX_LED_INTERNAL_CURVE_PATH_PROPAGATION_SUCCESS`
- `CODEX_LED_FLAT_ROW_OVERRIDE_FOLD_SUCCESS`
- `CODEX_LED_OCTAGON_CAP_OVERRIDE_SUCCESS`

Those tests spawn runtime `ATSAVLEDWall` actors through Unreal, inspect generated procedural mesh vertices/UVs, validate curve radii and seam continuity, test legacy radius migration, verify planar row overrides, and destroy the temporary actors. No LED geometry algorithm was changed in Phase 1.

## MCP baseline

The project was opened in a hidden UE editor with `-ModelContextProtocolStartServer`; the MCP endpoint was initialized successfully and exposed the Editor, Scene, Asset, Blueprint, Object, Automation, UMG, and TSAV toolsets.

Verified through MCP:

- current and complete five-map inventory;
- per-map actor counts and actual actor classes;
- TSAV project and plugin asset registries;
- `/Game/TSAV/App/L_TSAV_App` creation, load, WorldSettings, and save;
- live `GetDMXStatus()` response.

DMX status at baseline:

| Field | Value |
|---|---|
| Send enabled | true |
| Receive enabled | true |
| Protocols suspended | false |
| Input | `InputPort1`, Art-Net, registered, universes 1–10 |
| Output | `OutputPort1`, Art-Net broadcast, registered, universes 1–10 |
| Refresh rate | 44 Hz |
| Active/buffered universes during inspection | 0 / 0 |

## Packaging findings and known warnings

- The first cook exposed a missing `GameFeatureData` Asset Manager rule. `DefaultGame.ini` now supplies the required always-cook rule and subsequent BuildCookRun passes complete.
- The first packaged launch exposed constructor-only asset lookup in the application environment. The runtime path now uses `LoadObject`, and subsequent launches complete normally.
- The enabled CommonUI plugin reports that the project viewport client is not a `CommonGameViewportClient`. The Phase 1 shell does not use CommonUI; plugin allowlisting should either remove it or configure its viewport integration.
- A rendered Development launch on this machine reports a handled UE 5.8.1 TSR shader-permutation precache ensure. The application continues running; the NullRHI path reaches the map and exits with status 0 without a fatal or TSAV runtime error.
- MetaHuman optional content is not installed; the editor reports limited MetaHuman Creator features.
- NDI sources can report unavailable streams when the named external sender is offline.
- OpenXR reports an invalid API-version warning under the NullRHI commandlet environment.
- Non-target SDK validation reports Android, Linux, LinuxArm64, and VisionOS unavailable; Win64 is valid.

These warnings are recorded rather than treated as Phase 1 failures. Production packaging should reduce the enabled plugin set and remove unused rendering, XR, CommonUI, editor, and future-integration features from the shipping target.

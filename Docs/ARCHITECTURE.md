# TSAV PreVis architecture

## Product boundary

The packaged Unreal project is the TSAV PreVis desktop application. Codex, Unreal MCP, `TSAVPrevisTools`, and editor builders are development-time systems and are not dependencies of the shipped application.

```text
TSAV core/runtime behavior
          |
          +-- Runtime UMG application UI (shipped product)
          |
          +-- Editor builders and MCP diagnostics (development only)
```

Business logic belongs in runtime modules. Editor code authors or diagnoses runtime data. UMG exposes runtime behavior to operators. MCP exposes high-level editor diagnostics to Codex.

## Module layout

| Module | Type | Responsibility |
|---|---|---|
| `TSAVLEDTools` | Runtime | LED panels/walls, media surfaces, panel definitions, and articulated DMX fixtures |
| `TSAVLEDToolsEditor` | Editor | LED wall builder and GDTF/DMX fixture builder Slate tools |
| `TSAVPrevisRuntime` | Runtime | Packaged application shell, navigation, modes, scene identity, selection, and project state |
| `TSAVPrevisTools` | Editor | MCP/Toolset Registry diagnostics such as `GetDMXStatus()` |

`TSAVPrevisRuntime` depends on the existing `TSAVLEDTools` runtime module. It does not depend on `UnrealEd`, `AssetTools`, `LevelEditor`, `PropertyEditor`, `ToolMenus`, `TSAVLEDToolsEditor`, or `TSAVPrevisTools`.

## Runtime application shell

The first shell is implemented without a Blueprint dependency so it can be compiled, cooked, and tested before runtime widget assets are introduced:

| Runtime type | Role |
|---|---|
| `ATSAVAppGameMode` | Selects the runtime controller/pawn and bootstraps the application scene |
| `ATSAVPlayerController` | Owns viewport navigation, selection, gizmo interaction, keyboard commands, and the main UI |
| `ATSAVEditPawn` | Free-flying design viewport camera |
| `UTSAVMainWidget` | C++ UMG desktop chrome with commands, scene outliner, editable inspector, and status bar |
| `UTSAVCommandSubsystem` | Command history for spawn, delete, duplicate, transform, and scene-object properties |
| `ATSAVTransformGizmoActor` | Runtime translate, rotate, and scale handles with local/world space |
| `UTSAVProjectSubsystem` | Active project state plus versioned `.tsav` JSON save/load |
| `UTSAVModeSubsystem` | One top-level mode over the shared scene |
| `UTSAVSelectionSubsystem` | Runtime local-player selection independent of Unreal Editor selection |
| `UTSAVSceneObjectComponent` | Persistent object GUID, display name, semantic type, lock, and visibility |
| `ITSAVSelectable` | Extensible runtime selection contract |
| `ATSAVSceneObjectActor` | Minimal selectable object used by the application-shell smoke scene |

The input contexts `IMC_TSAV_Common` and `IMC_TSAV_Edit` are created as transient runtime objects for the first shell. This avoids blocking the executable on new binary Input Action assets. They can later be replaced with content assets without changing controller behavior.

The first packaged acceptance pass verifies the actual application window, focused-viewport flight, and runtime click selection. Desktop input explicitly disables the legacy touch interface.

## Startup

| Setting | Value |
|---|---|
| Packaged game map | `/Game/TSAV/App/L_TSAV_App` |
| Editor startup map | `/Game/VprodProject/Maps/Main` |
| Default game mode | `/Script/TSAVPrevisRuntime.TSAVAppGameMode` |

`L_TSAV_App` was created and inspected through Unreal MCP. Its WorldSettings also explicitly selects `ATSAVAppGameMode`. The map provides the basic floor/light environment; the GameMode fills in missing shell elements and selectable smoke objects at runtime.

## Existing runtime systems retained

The following `TSAVLEDTools` types remain the authoritative implementations and must not be recreated in the application module:

- `ATSAVLEDWall`
- `ATSAVLEDPanel`
- `ATSAVMediaSurfaceActor`
- `ATSAVDMXFixture`
- `UTSAVLEDPanelDefinition`

One packaging correction was required: `ATSAVLEDWall::RebuildPanelLayout()` now uses `RerunConstructionScripts()` only in editor builds and calls the existing runtime media/geometry refresh functions in non-editor builds. No geometry algorithm was changed.

## Video-feed architectural decision

The next cross-cutting runtime design is a source-neutral video feed abstraction:

```text
Media / NDI / Camera / Test Pattern / Switcher
                       |
                 UTSAVVideoFeed
                       |
              Video router destinations
                       |
             LED wall / monitor / output
```

`UTSAVVideoFeed` should expose a display texture, resolution, display name, liveness, and durable endpoint ID. `ATSAVMediaSurfaceActor` should retain its existing `UMediaSource` path for compatibility while gaining an optional feed/texture path. This change belongs after the runtime shell and selection foundation are verified; it should not be implemented as a one-off camera-to-material assignment.

## Runtime authoring and persistence

The primary project format is a versioned structured `.tsav` JSON document. Runtime scene objects use stable `FGuid` identities; actor instance names are not durable references. Version 1 stores the project ID/name and each scene object's class path, semantic type, transform, display name, lock state, and visibility. Save/load restores those identities and clears command history at the document boundary. The format reserves `videoRoutes` and `settings` sections for later phases. Unreal `USaveGame` remains reserved for local application preferences, recent files, and recovery metadata.

Gizmo drags are transactions: continuous pointer updates preview the transform, while pointer release adds one transform command. Inspector edits, lock/visibility changes, spawn, duplicate, and delete use the same command history, so all supported scene mutations have symmetric undo and redo behavior.

## Standalone packaging boundary

`LiveEventTest.uproject` disables engine plugins by default and explicitly enables only the two TSAV runtime plugins. `LiveEventTestEditor.Target.cs` owns the broad development authoring stack. Because Unreal cooking runs in an editor host, `Build/Package-TSAVPreVis.ps1` additionally disables editor-only top-level plugins for the cook and explicitly cooks `/Game/TSAV/App/L_TSAV_App`. The Windows runtime config uses `UGameEngine` and `UGameViewportClient`; legacy nDisplay engine overrides no longer leak into the executable.

## Phase boundaries

1. Baseline inventory and reproducible build evidence.
2. Packaged runtime application shell.
3. Runtime transforms, command-based undo/redo, inspector editing, and `.tsav` persistence.
4. Runtime LED wall UI around the existing LED actors.
5. Runtime lighting library, followed by a separate runtime GDTF importer.
6. Venue, stage, and truss authoring.
7. Cameras, source-neutral routing, and virtual switcher.
8. Walkthrough characters and curated MetaHumans.
9. Production Development/Shipping packaging and smoke automation.

No phase is complete until its acceptance workflow is reproducible in a packaged build.

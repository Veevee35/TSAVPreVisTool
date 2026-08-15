# Build progress

Updated: 2026-08-15.

## Phase 0 — baseline and documentation

| Deliverable | Status |
|---|---|
| Source-code/module inventory | Complete |
| Unreal asset and level inventory through MCP | Complete |
| Plugin dependency graph | Complete |
| Runtime/editor classification | Complete |
| Baseline editor build | Pass |
| Existing LED validation evidence | Pass (six prior Unreal assertions) |
| Live DMX diagnostic | Pass; ports registered, no active input packets during inspection |
| NDI/media live-sender acceptance | Not reproducible without an active external sender |

## Phase 1 — runtime application shell

| Deliverable | Status |
|---|---|
| `TSAVPrevisRuntime` runtime plugin | Complete |
| `ATSAVAppGameMode` | Complete |
| `ATSAVPlayerController` | Complete |
| `ATSAVEditPawn` | Complete |
| `UTSAVProjectSubsystem` | Complete (identity/dirty-state shell) |
| `UTSAVModeSubsystem` | Complete |
| `UTSAVSelectionSubsystem` | Complete (single/multi selection data model; click trace uses primary selection) |
| `ITSAVSelectable` / `UTSAVSceneObjectComponent` | Complete |
| Runtime UMG main shell | Complete in C++ as `UTSAVMainWidget` |
| Dedicated `/Game/TSAV/App/L_TSAV_App` | Complete through MCP |
| Enhanced Input context separation | Common/Edit contexts implemented at runtime; Camera/Walkthrough/Video contexts are future phases |
| Editor compile | Pass |
| Non-editor Win64 compile | Pass |
| Win64 Development package | Pass; one-map BuildCookRun archived to `Saved/Packages/Win64Development` |
| Packaged launch and clean-exit smoke | Pass; exit code 0 under NullRHI unattended launch |
| Packaged window/UI smoke | Pass; desktop chrome verified from direct window capture |
| Packaged input smoke | Pass; click selection/inspector update and focused-viewport W navigation verified |

## Implemented operator behavior

- Packaged game starts in the dedicated TSAV application map.
- Runtime GameMode supplies an edit pawn and custom player controller.
- RMB + mouse looks; WASD flies; Q/E moves vertically; mouse wheel adjusts fly speed.
- LMB performs a runtime visibility trace and selects TSAV-selectable objects.
- Each scene object has a durable `FGuid` and common display/type/lock/visibility state.
- The UMG shell exposes File/Edit/Build/LED/Lighting/Video/Camera/View headings, project outliner, ten authoring-mode buttons, inspector, and status strip.
- Mode changes flow through one subsystem and swap Enhanced Input contexts.
- F1/Escape returns from the placeholder Walkthrough mode to Select.

## Explicitly not complete

Phase 1 does not claim the Phase 2 acceptance workflow. Runtime transform gizmos, command-based undo/redo, property editing, deletion/duplication, `.tsav` serialization, and recovery are not implemented yet. The runtime UI is a C++ `UUserWidget`; a designable `WBP_TSAV_Main` subclass can be introduced through MCP after the shell behavior stabilizes.

## Next implementation sequence

1. Add the runtime Interactive Tools Framework transform gizmo.
2. Add command-based undo/redo before expanding authoring tools.
3. Add versioned `.tsav` save/load with GUID-based object references.
4. Add automated acceptance coverage for spawn, select, transform, undo/redo, save, reload.
5. Build the runtime LED wall panel around `ATSAVLEDWall`.
6. Introduce `UTSAVVideoFeed` and migrate LED surfaces with backward-compatible `UMediaSource` support.
7. Define a shipping plugin allowlist and remove editor/future-integration payload from the packaged target.

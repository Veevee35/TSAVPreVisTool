# Independent TSAV feature development

Direction: build original TSAV implementations that run with SuperStage disabled. The vendor plugin remains an optional integration, not a dependency or a source of unlocked features. Its installation is not a completed implementation of those features in TSAV.

## Use native lighting now

1. Close the editor and launch `Start-TSAVPreVis.cmd`. This opens the native TSAV application map with SuperStage disabled. The launcher accepts `-Map` for another native scene; it does not convert a level containing SuperStage actors.
2. Open **Tools > TSAV Lighting Console**. Select a library model and click **Place selected library fixtures**, or use the existing fixture builder.
3. Use **Select Placed** to select scene instances. Enter **Universe** and **Start address**, then **Patch selected in list order**. TSAV creates individual patches for those lights, preserving the library template. Batches roll onto the next universe and reject occupied scene addresses.
4. Adjust the common programmer or the primary fixture's raw attributes. Dimmer, RGB, pan, tilt and zoom drive TSAV's own actor/light components. Blackout and grand master apply to the selection.
5. **Save All** saves the level's fixture bindings and the DMX library containing the new patches. Patch batches support editor Undo/Redo. Scene patch edits are independent of the catalog's default addresses.

Fixtures sharing an address initially respond together; assigning individual addresses separates them. Library rows still edit shared templates. Select Placed selects only actual scene instances, so templates aren't patched a second time. Local preview works without external output; transmitting DMX additionally requires output ports covering the chosen universes.

The instance patch API lives in the runtime `ATSAVDMXFixture` class. The console UI in this change is an Unreal Editor panel. A packaged console UI, durable lighting show data, and fixture-specific `.tsav` restoration remain separate work.

## Feature coverage and remaining work

This is a workflow inventory, not a claim of full parity. Public [fixture-system](https://yunsio.com/docs/fixture) and [console documentation](https://yunsio.com/docs/console) supply the comparison categories; TSAV status below comes from its own source. No vendor binaries, activation logic, shaders, meshes, or proprietary data are being converted into TSAV implementations.

| Workflow | TSAV implementation found | Remaining independent implementation |
|---|---|---|
| Fixture catalog, GDTF authoring | `TSAVDMXFixtureCatalog`, `STSAVDMXFixtureBuilder`, existing project fixture assets | Runtime import, broader validation and explicit support for complex multi-emitter profiles |
| Individual patching | Native scene rows and `ATSAVDMXFixture::SetIndividualPatchAddress` | Packaged patch UI and complete lighting project persistence |
| Basic fixture response | TSAV pan/yoke/head, spotlight, RGB, dimmer and zoom | Beam/optics capability model, per-mode physical ranges and multi-head evaluation |
| Lighting programmer | Native editor common controls, raw attributes, selection, blackout and grand master | Runtime UI, persistent programmer state and release/priority rules |
| Groups and presets | No TSAV lighting implementation found | Store/recall selected fixture IDs and sparse attribute values |
| Cues and executors | No TSAV lighting implementation found | Cue stacks, timing, tracking, fade engine, playback arbitration and operator UI |
| Effects and timecode | No TSAV lighting implementation found | Waveform effects, phase/spread, timelines and synchronized playback |
| Advanced optics | Basic Unreal spotlight only | Gobo/color wheels, prisms, shutters/strobe, frost, framing and volumetric effects |
| DMX input/output | Existing Unreal DMX runtime and project ports | Operator diagnostics, protocol/adapter UI and external-device acceptance |
| DMX recording | Engine authoring tools may be enabled; no TSAV recorder | TSAV recording format, playback transport and persistence |
| Fixture arrays and pixel control | LED wall layout exists; no lighting matrix engine | Instance arrays, fixture cells and DMX pixel mapping |
| Stage/venue/truss authoring | Runtime scene objects and basic stage/truss placeholders | Parametric geometry, connection/snapping rules and meaningful dimensions |
| Scenic systems | No specialized TSAV drape/scaffold/barrier/crowd generators found | Original procedural tools and independently sourced assets |
| Screens and LED walls | `TSAVLEDWall`, `TSAVLEDPanel`, media surfaces, configurator | Broader projector/surface mapping and acceptance with live inputs |
| Cameras/video/NDI | `TSAVCameraActor`, `TSAVVideoSwitcher`, existing media/NDI path | Cross-source routing coverage, input liveness and hardware validation |
| Lasers/ILDA | No TSAV implementation found | Original animation/geometry, preview, format support and operator tools |
| Machinery and stage effects | No TSAV implementation found | Lift/rail systems, fountains and effect-machine simulation |
| MVR/MA data exchange | Engine editor plugins may be available; no TSAV exchange workflow | Import/export adapters with round-trip identity, transforms and patch tests |
| Atlas/VAT authoring | No TSAV-specific implementation found | Original texture/animation tooling with deterministic exports |
| Show/project files | `.tsav` scene persistence and selected actor-specific serializers | Lighting configuration, groups, cues and effect state must join the same project format |

## Implementation sequence

1. Native per-instance lighting control and patching; verify with SuperStage unloaded.
2. Runtime show document and fixture serialization, then groups/presets/cues. Keep editor and packaged UI on the same runtime model.
3. Expand fixture capabilities and render representative moving head, wash, matrix and profile fixtures.
4. Add stage/truss/scenic generators and integrate existing screen/video tools.
5. Add effects, recording/timecode and file exchange, followed by specialized laser/machinery tooling.

Each item needs its own end-to-end acceptance check. Full feature equivalence is substantial product development and is not complete after the first lighting change.

## Validation

Build the editor and run `Build/Test-TSAVLighting.ps1`. Its isolated Entry-map tests create an original seven-channel profile without loading vendor content, verify that SuperStage modules are unloaded, and test separate patches, overlap checks, Undo/Redo, selection, fader rebuilding, partial attribute updates, and actual native spotlight intensity under dimmer/blackout.

These are component/output-state tests under NullRHI, not screenshots or external network/hardware acceptance. The test does not save the user's map or repack the fixture catalog.

Verified on 2026-09-05: editor and Win64 Development game targets compiled; both native tests passed with all ten SuperStage modules unloaded. Local report: `Saved/NativeLightingReview/Run-316a259872604fa7a783b811b6345607/index.json`. This compile does not update an already archived packaged application.

# Native TSAV lighting and show tools

TSAV owns these implementations. They run with SuperStage disabled and do not require SuperStage activation. The optional vendor integration is a separate editor workflow. This is the supported TSAV feature set and operator guide, **not a claim of complete SuperStage feature parity**.

## Start and find the tools

Launch `Start-TSAVPreVis.cmd` to open the native application map in Unreal Editor. It accepts `-Map` for another native level and `-Build` to compile first. It does not convert SuperStage actors in an existing level.

| Workflow | Unreal Editor Tools menu | Standalone application |
|---|---|---|
| Place and patch | TSAV Lighting Console | Lighting > Add DMX Fixture, then Lighting Show Console |
| Programmer and playback | TSAV Lighting Show; also linked from Lighting Console | Lighting > Lighting Show Console |
| Arrays and interchange | TSAV Fixture Arrays and Rig Exchange | Lighting > Fixture Arrays and MVR / GDTF |
| Protocol setup | TSAV DMX Connections and Monitor | Lighting > DMX Connections and Monitor |
| Scenery and machinery | TSAV Stage and Scenic Builder | Build > Stage and Scenic Builder |
| Laser preview | TSAV Laser Preview and ILDA | Lighting > Laser Preview and ILDA |

The new editor and standalone panels share runtime implementations. Existing archived executables must be rebuilt. `Build/Package-TSAVPreVis.ps1` defaults to Shipping; `-Configuration Development` creates a diagnostic build.

## Place and patch lights

1. In the editor Lighting Console, choose a library fixture and **Place selected library fixtures**. In the standalone app use **Lighting > Add DMX Fixture**. The rig panel also accepts a GDTF profile or an existing scene fixture.
2. Select actual scene fixtures in the Lighting Show Console. Enter **Universe** and **Start address**, then **Patch selected consecutively**.
3. The planner uses each mode's footprint, rolls onto the next universe when needed, and rejects occupied scene addresses before changing fixtures.
4. Move dimmer, RGB, pan, tilt and zoom controls. Native light components respond without a network output port. Further faders expose the primary mode, optics and matrix cells.
5. Save the level in the editor or the `.tsav` project in the app. Configuration, mode definitions, per-instance addresses and fixture IDs survive reload.

Individual patches preserve catalog templates. Fixtures deliberately sharing addresses can respond together; repatching separates them. The older editor console still exposes library-template rows, while **Select Placed** selects scene instances only. In an optional licensed SuperStage session, the existing patch adapter remains available. Native playback controls TSAV actors; it does not activate vendor actors.

### Arrays

Choose a scene fixture or read a GDTF file and select a supported mode. Set rows, columns, spacing, origin, yaw and starting patch. **Place and patch fixture array** creates up to 512 independently identified fixtures in row order. It checks occupied addresses and supports batch undo.

## Program a show

`ATSAVLightingShow` is created in the scene when needed. It stores groups, presets, cues, effects, executor definitions, timeline events and recordings. References use persistent fixture IDs, so moving or renaming a fixture does not break the show.

- **Groups:** select fixtures, enter a positive slot number and optional name, then Store/replace. Recall restores selection.
- **Presets:** set attributes and store selected fixtures. Recall puts the preset into the programmer.
- **Store scope:** click to cycle All, Programmer, Dimmer, Color, Position and Beam. Programmer stores explicitly entered attributes only; other scopes filter the current base look.
- **Cues:** set fade, delay, follow hold (`-1` means manual) and tracking, then store. GO runs the chosen cue; Next and Previous navigate the stack. Pause, Stop and Seek control playback. Tracking keeps earlier cue values; an untracked cue starts from captured fixture baselines.
- **Release programmer:** programmer attributes override playback until released. Presets/cues store base looks before master/blackout. Effects are captured by the output recorder.
- **Master / blackout:** applied once to final dimmer output for show-controlled fixtures. The older console's selection master is a separate control.
- **Undo show / Redo show:** restore show-data snapshots. Runtime scene commands separately support patching, configuration, scene creation and array undo/redo.

### Executors and effects

Store an executor with a comma-separated cue sequence such as `1,3,7`. Recall advances that executor independently; each has a level and optional loop. Cue timings and follow settings apply. Among executor layers, dimmers merge at the highest value and the latest GO wins other attributes. Executor-controlled values overlay the main stack; programmer values remain on top. Release an executor to expose underlying playback. Global Stop releases all playback layers and effects.

Effects accept an attribute, waveform (0 sine, 1 triangle, 2 square, 3 saw), period, normalized minimum/maximum and phase spread in cycles across fixture order. Store selected fixtures in a slot, then Start. Effects use a deterministic show clock.

### Timeline and timecode

Choose a cue and time in seconds, then **Schedule selected cue number at time**. Play, stop and seek the timeline. Seeking reconstructs tracking and fades, including backward seeks.

For external synchronization, configure an Unreal timecode provider, enable **Follow Unreal's synchronized timecode provider**, set the offset, and play the timeline. An offset of `-3600` maps incoming `01:00:00` to show time zero. The panel reports synchronization and holds playback if the provider is unavailable. TSAV consumes Unreal's provider; it does not decode raw LTC audio. Physical timecode acceptance remains unverified.

### Output recording

Select fixtures and a recording slot, choose 1-60 samples/second and Record. **Stop recording / keep take** stores it. Playback supports pause, seek and release. Saves include an in-progress recording snapshot. Limits are 100,000 frames per take and two million attribute samples across the show's recordings; a full buffer stops recording.

This records normalized native fixture output before master/blackout, including effects and DMX input reflected by those fixtures. It is not a packet capture of every network universe.

## Optics, matrices and pixel mapping

Native optics include shutter, strobe, additive white/amber, subtractive CMY, CTO, configurable color wheel, iris, frost, four original procedural gobos, rotation/spin, four framing blades, and a split-intensity prism with adjustable facets/separation. Light functions and Unreal volumetric scattering provide the preview. Projected optics use conventional lighting to retain their masks in UE 5.8 camera captures; large projected rigs can cost more GPU time than plain MegaLights. Aerial beams need suitable fog in the scene; shadow/scattering settings are configurable.

Matrix modes create separate RGB/dimmer lights. `cell[x,y].attribute` values participate in the programmer, cues, master, blackout, recordings, project files and DMX I/O. This previews RGB/dimmer cells, not independently articulated moving heads.

**Image pixel mapping:** select fixtures in row order, enter an image path and column count, then Map. Pixel centers are sampled as linear RGB; alpha controls dimmer. A single matrix receives the image across its cells. Store the resulting programmer look as a preset or cue. Limits are 64 MB and 16 million pixels. Live video-to-DMX mapping is not implemented.

These are generic TSAV optics, not every manufacturer's physical channel-capability model, photometry or wheel artwork. Supported GDTF pan, tilt and zoom physical ranges are applied on import.

## DMX connections and monitor

The panel lists local IPv4 adapters and configured Unreal ports. Choose an existing port or New port. Set input/output, Art-Net or sACN, adapter, universe mapping, destination and loopback; **Apply port and connect fixtures** updates native scene libraries. Failed registration rolls back. Local universes begin at 1; Art-Net external universes may begin at 0, sACN at 1. Output uses unicast when selected, otherwise multicast/broadcast according to protocol.

**Save port settings on this computer** persists machine-specific settings separately from show data. The form represents one output destination: applying it to a multi-destination port replaces that destination list.

Local preview always works independently of output ports. Enable **Network DMX output** in the show console to transmit. The monitor shows latest buffered channels 1-32 for a local universe; buffered data is not proof of ongoing packet arrival. Physical fixture and console network acceptance remains unverified.

## GDTF and MVR fixture-rig interchange

Runtime GDTF import uses Unreal's public format types with TSAV validation. Supported modes use one DMX break and contiguous ascending/descending channel bytes, 8-32 bit. Defaults and supported physical ranges are applied. Geometry-reference, multi-break and split-channel modes are reported as unsupported. Runtime import uses generic TSAV fixture geometry; use the existing editor fixture builder for authored models.

**Inspect MVR** reads embedded profiles and the fixture rig. **Import inspected rig** stages fixtures with IDs, hierarchy-composed transforms, modes and patches. Duplicate IDs, including IDs already in the scene, are rejected before creating a duplicate rig. This is add-only import, not merge/update. Original addresses, including shared addresses, are retained; array/repatch tools provide occupancy checking when unique addresses are wanted.

**Export all scene fixtures to MVR** writes identities, poses and patching with embedded profiles. Imported original GDTF bytes are retained for re-export. Native single-emitter modes can generate original GDTF descriptions. Native matrix profiles cannot be synthesized for export without an original GDTF.

Coordinate conversion handles right-handed Z-up millimeters versus Unreal centimeters/handedness. Unsupported shear is rejected. This transfers **fixture rigs**: arbitrary scenic MVR resources are not preserved in exported files, and proprietary MA showfile/XML formats are unsupported. Format references: [GDTF developers](https://gdtf-share.com/help/developers/) and [MVR definition](https://gdtf-share.com/help/developers/mvr_1_6/file-format-definition/index.html).

## Scenery and machinery

The builder supplies dimensioned stage decks, stairs, straight truss, truss rings, scaffolds, pleated drapes, barriers, crowd figures, lifts and rails. Dimensions are centimeters. Set counts, dimensions, placement and grid snapping, then Create. Select an existing object to edit geometry while preserving placement; use scene transforms to move it.

Lifts/rails support normalized travel and cyclic animation. Files preserve settings and position; animation phase restarts when reconfigured/reloaded. These are original parametric previews, not engineering/load calculations. Truss endpoint connection editing, complete venue generation, fountains and effect-machine simulation remain unimplemented.

## Laser preview and ILDA

Create a projector and original circle, fan or Lissajous animation. Set width, height, distance, line width, intensity and frame rate. Move/aim it using scene transforms. Rays, looping and following the show timeline are optional. Playback supports play, pause and seek. Project saves include settings and frame data.

The independent ILDA reader supports indexed 2D/3D frames (0/1), palettes (2), true-color 3D/2D frames (4/5), blanking and projector numbers. Export supports true color or indexed color (at most 256 colors/frame). Invalid/truncated files leave the current animation intact. Limits are 64 MB and one million points. Format 3 is unsupported. This is visualization/file interchange, not laser-hardware output. Reference: [ILDA transfer specification](https://www.ilda.com/resources/StandardsDocs/ILDA_IDTF14_rev011.pdf).

## Files and remaining coverage

- `.tsav` project files include identities, transforms, fixture configuration/mode/patch, show data, generated scenery and laser data. Loading stages and validates replacements before removing current actors. Rejected files preserve the current project. Saves use a temporary file followed by replacement.
- `.tsavshow` contains show data tied to fixture IDs; it neither creates missing fixtures nor includes geometry. Load it into the corresponding rig.
- Existing LED walls/panels, curved surfaces, video switching, cameras, media and NDI remain available. This work does not establish new input-liveness or physical-device acceptance.
- Atlas/VAT authoring, live video pixel mapping, complete complex-GDTF visualization, arbitrary scenic MVR round trips, proprietary MA formats, and specialized scenery/effects listed above remain future independent work. No vendor feature is unlocked by installing TSAV.

## Validation

After building the editor, run `Build/Test-TSAVLighting.ps1`. Add `-Render` for GPU captures instead of NullRHI. Tests isolate the Entry map or a dedicated test world and assert SuperStage modules are unloaded. They do not save the user's map or rebuild the fixture catalog.

Ten checks cover patch planning, native fixture output, show playback, project persistence/undo, matrices, scenery, ILDA, rig exchange, executors/timecode/image mapping and render review. Failure cases include occupied patches, malformed modes/files, duplicate IDs, wrong versions, unsupported transforms and truncated ILDA.

Reports use unique `Saved/NativeLightingReview/Run-.../index.json` paths. GPU captures include `NativeShowConsole.png`, `NativeOpticsAndLaser.png`, `NativeScenicBuilder.png`, `NativeLaserPanel.png`, `NativeRigPanel.png` and `NativeNetworkPanel.png`. Inspect the optics/laser image alongside automation results: writing a screenshot alone does not prove visual correctness. Local tests are not external-device acceptance.

### Verified build — 2026-09-05

- UE 5.8.1: Editor and Win64 Development game compiled successfully.
- Native regression report: `Saved/NativeLightingReview/Run-363a0a27f5944e57be45b3a3474177a4/index.json` — ten successes, no failed tests or test warnings. Render assertions verify distinct projected gobo bands and independently colored laser segments.
- Development build/cook/stage/archive succeeded: `Saved/native-suite-package.log`. The native gobo and laser materials are included; the packaged asset manifest has zero SuperStage entries.
- Launch the new package at `Saved/Packages/NativeSuiteDevelopment/Windows/LiveEventTest.exe`.
- Packaged runtime smoke report: `Saved/NativePackageReview/Run-d2d9809a3127487e90d29ce196e08df1/packaged-smoke.log`. Project replacement, undo, camera/switcher routing and LED configuration/binding passed. Repeat with `Build/Test-TSAVPackage.ps1` against a Development archive. This smoke check uses NullRHI; rendered optical checks are in the native GPU suite above.
- Project replacement now retains switcher routes when an old camera is retired in favor of a staged camera with the same provider identity. A regression check covers reloading over an existing camera/switcher scene.
- The standalone process omits the editor-only GameFeatures asset scan when that optional plugin is disabled, without rewriting the user's configuration.

The cook still reports an existing missing material dependency in the Robe Robin Footsie1 Slim RGBW catalog mesh. This change does not rebuild that catalog asset. Full vendor parity and hardware acceptance remain limited as described above.

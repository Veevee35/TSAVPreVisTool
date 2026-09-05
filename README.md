# TSAV PreVis Tool

Unreal Engine tools for A/V production previsualization.

## Native TSAV show tools

Launch **Start-TSAVPreVis.cmd** for the native workflow with SuperStage disabled. **Tools > TSAV Lighting Console** places and patches individual fixtures; its **Open Lighting Show** button opens the shared programmer and playback console. In the standalone application, use **Lighting > Lighting Show Console**.

Native additions include groups, scoped presets, tracking cues, independent executors, fades, effects, timecode/timeline transport, output recording, optics, matrix cells, image pixel mapping, DMX connection setup, fixture arrays, GDTF/MVR fixture-rig interchange, parametric scenery/machinery, and ILDA laser preview. They join project persistence and runtime undo. See the [operator guide and supported formats](Docs/TSAV_NATIVE_FEATURES.md), including remaining limitations. Full vendor feature parity is not complete.

After compiling the editor, run `Build/Test-TSAVLighting.ps1`. Add `-Render` for panel and optics captures. Create an updated standalone executable with `Build/Package-TSAVPreVis.ps1`; archived builds do not update automatically.

The current native Development package is under `Saved/Packages/NativeSuiteDevelopment/Windows/LiveEventTest.exe`. `Build/Test-TSAVPackage.ps1` checks its runtime project persistence, undo and video routing in an isolated session. Build/test results are recorded in the operator guide.

## Optional SuperStage editor integration

Launch **Start-TSAVSuperStage.cmd** to open TSAV with the complete locally installed SuperStage 26H2.6 suite. **Tools > TSAV SuperStage** exposes its registered panels and content library alongside the existing TSAV tools. Activate licensed features through SuperStage's own sign-in panel. See [setup, validation, and runtime limitations](Docs/SUPERSTAGE_INTEGRATION.md).

The supplied archive contains editor binaries without C++ build rules or standalone libraries. The launcher enables it at editor startup; normal TSAV builds and packaging keep it disabled. A vendor runtime SDK is required to include SuperStage in the standalone executable. Restore the local vendor files with `Build/Install-SuperStage.ps1 -ArchivePath <archive>`.

## Standalone TSAV PreVis application

The packaged Unreal project is a runtime previs application, not an editor remote control. Phase 2 adds a runtime scene outliner and inspector, translate/rotate/scale gizmos, command-based undo/redo, object duplication/deletion, and versioned `.tsav` project save/load.

Runtime controls:

- `W`, `E`, `R`: translate, rotate, and scale gizmo modes
- `X`: toggle local/world gizmo space
- `Ctrl+Z`, `Ctrl+Y`: undo and redo
- `Ctrl+D`, `Delete`, `Insert`: duplicate, delete, and add a cube
- `Ctrl+S`, `Ctrl+O`: save and load the default `.tsav` project
- Hold right mouse and use `WASD`, `Q`, `E`: fly the design camera
- Shift-click: additive selection

Create a clean Windows package with `Build/Package-TSAVPreVis.ps1`. It defaults to Shipping, cooks only the application map, and filters editor/Codex authoring plugins out of the cook.

## Camera and video switcher editor tools

Open **Tools > TSAV Camera Tool** in Unreal Editor to name a camera input, choose its output resolution and lens, mark it as fixed or PTZ, and create it from the current editor view. Its rendered feed is automatically registered with every video switcher in the level and appears as a routable camera source. Open **Tools > TSAV Camera Controller** to assign up to four project cameras to independent control banks. Every numeric pan, tilt, zoom, iris, focus, gain, world-position, VISCA port, and movement-speed setting provides both a visible slider and a typeable value. Sliders update the Unreal camera continuously while dragged; VISCA-enabled banks send rate-limited live commands during the move and a final exact command on release. Typed values and connection fields apply automatically when committed, so the Apply buttons are not required. Open **Tools > TSAV Screen Control** to select any LED wall or panel and edit its name, brightness, canvas start X/Y, world location, and rotation from one panel. Open **Tools > TSAV Video Switcher** to create or select a switcher, discover current level cameras, Media Source assets, and visible NDI senders, and route them directly to Program, Preview, Aux 1, or Aux 2. Its **Video Wall Outputs** matrix discovers every media-capable LED wall or panel in the current level, shows the source currently reaching it, and assigns the wall to any switcher bus or back to its direct Media Source. The switcher panel also provides Cut, Auto, actor properties, and an optional manual stream/NDI source field.

## LED and NDI builder

The project includes reusable **TSAV LED Panel Definition**, **TSAV LED Panel**, and **TSAV LED Wall Builder** types in the `TSAVLEDTools` plugin. Open **Tools > TSAV LED Wall Builder** for the guided panel, wall, canvas, and NDI workflow. Open `/Game/TSAV/Levels/LED_Canvas_Configurator` for a ready-made level containing a center LED wall, two standalone panels, lighting, a floor, and a preview camera.

Panel definitions store real-world cabinet dimensions and native pixel resolution. The wall links that cabinet into rows and columns, calculates total screen resolution, generates serpentine cabinet topology records, and maps the resulting screen to an exact X/Y rectangle on a 4096×2160 processor canvas. Every column seam and row seam can bend from -90° to +90° in 0.5° steps, including simultaneous two-axis curvature and 90° under-folds for isolated enabled runs. Each column can also enable two independent signed circular arc sweeps inside every panel, each adjustable from -90° to +90° in 0.5° steps. The left half changes the outgoing wall heading, the right half continues from it, and every following column starts from the resulting connected edge and direction; positive and negative sweeps curve in opposite directions and can form S-curves. Existing radius-based walls migrate to equivalent angles automatically. Consecutive flat-row overrides form centered planar top or bottom surface groups from only their enabled panel footprint while retaining their row-seam orientation and using a seam-only reference path; empty columns no longer stretch the surface or inherit internal column arcs. Bends use shared video-plane corners so the front image remains watertight, with cabinet clearance and collision behind it; every cabinet side and exposed outer edge retreats inward and backward at 45° to avoid clipping tightly folded panels. An interactive paint grid supports square, disabled, full corner-to-corner diagonal, and corner-spanning rounded cabinets in all four directions, with click-drag rectangular selection for applying a shape to many panels at once. Rounded edge radius accepts arbitrary values from 0.5 m upward with up to ten decimal places. Disabled cells are removed from geometry and signal linking so irregular wall silhouettes can be built. The editor tool also includes a live canvas preview, overflow validation, an NDI/Media Source picker, Rectangle RGB, Round RGB, and Round Linear physical subpixel simulations, and buttons to create a new wall or update the selected one. In the standalone switcher, **Refresh Inputs** discovers visible NDI senders and adds them directly to the crosspoint list; manual URLs remain available as a fallback.

## GDTF and DMX fixture builder

Open **Tools > TSAV GDTF DMX Fixture Builder** to import a `.gdtf` definition. When the GDTF contains glTF/GLB model resources, the tool extracts them from the archive, imports the referenced meshes, assigns the base, yoke, head, and lens, scales each part to its declared GDTF dimensions, and assembles it from the GDTF geometry transforms. FBX, OBJ, glTF, and GLB can still be imported manually when the GDTF has no compatible embedded model. The tool creates the Unreal DMX fixture type, library, patch, and a functioning fixture actor with configurable model scale and rotation, pan/tilt pivots and limits, movement speed, beam orientation, zoom, color, dimmer, and live preview values.

Use **Tools > Build Complete GDTF Fixture Library** to rebuild `/Game/TSAV/Fixtures/DMX/DMX_TSAV_AllFixtures` and the runtime fixture catalog from every GDTF under `/Game/DMX/GDTF_Fixtures`. The batch imports glTF/GLB resources as combined static fixture parts, converts legacy embedded 3DS geometry to combined static meshes, and supplies dimensioned GDTF primitive parts when a profile has no authored model. It also allocates non-overlapping patches across as many universes as required and validates every model reference, patch footprint, pan/tilt response, dimmer/color/zoom beam response, and mesh bounds. In the standalone application, choose **Lighting > Add DMX Fixture** to search the generated catalog by manufacturer, fixture, mode, or revision and create the selected articulated, patched fixture.

See `Plugins/TSAVLEDTools/README.md` for the short workflow.

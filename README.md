# TSAV PreVis Tool

Unreal Engine tools for A/V production previsualization.

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

Open **Tools > TSAV Camera Tool** in Unreal Editor to name a camera input, choose its output resolution and lens, mark it as fixed or PTZ, and create it from the current editor view. Its rendered feed is automatically registered with every video switcher in the level and appears as a routable camera source. Open **Tools > TSAV Screen Control** to select any LED wall or panel and edit its name, brightness, canvas start X/Y, world location, and rotation from one panel. Open **Tools > TSAV Video Switcher** to create or select a switcher, discover current level cameras, Media Source assets, and visible NDI senders, and route them directly to Program, Preview, Aux 1, or Aux 2. Its **Video Wall Outputs** matrix discovers every media-capable LED wall or panel in the current level, shows the source currently reaching it, and assigns the wall to any switcher bus or back to its direct Media Source. The switcher panel also provides Cut, Auto, actor properties, and an optional manual stream/NDI source field.

## LED and NDI builder

The project includes reusable **TSAV LED Panel Definition**, **TSAV LED Panel**, and **TSAV LED Wall Builder** types in the `TSAVLEDTools` plugin. Open **Tools > TSAV LED Wall Builder** for the guided panel, wall, canvas, and NDI workflow. Open `/Game/TSAV/Levels/LED_Canvas_Configurator` for a ready-made level containing a center LED wall, two standalone panels, lighting, a floor, and a preview camera.

Panel definitions store real-world cabinet dimensions and native pixel resolution. The wall links that cabinet into rows and columns, calculates total screen resolution, generates serpentine cabinet topology records, and maps the resulting screen to an exact X/Y rectangle on a 4096×2160 processor canvas. Every column seam and row seam can bend from -90° to +90° in 0.5° steps, including simultaneous two-axis curvature and 90° under-folds for isolated enabled runs. Each column can also enable two independent signed circular arc sweeps inside every panel, each adjustable from -90° to +90° in 0.5° steps. The left half changes the outgoing wall heading, the right half continues from it, and every following column starts from the resulting connected edge and direction; positive and negative sweeps curve in opposite directions and can form S-curves. Existing radius-based walls migrate to equivalent angles automatically. Consecutive flat-row overrides form centered planar top or bottom surface groups from only their enabled panel footprint while retaining their row-seam orientation and using a seam-only reference path; empty columns no longer stretch the surface or inherit internal column arcs. Bends use shared video-plane corners so the front image remains watertight, with cabinet clearance and collision behind it; every cabinet side and exposed outer edge retreats inward and backward at 45° to avoid clipping tightly folded panels. An interactive paint grid supports square, disabled, full corner-to-corner diagonal, and corner-spanning rounded cabinets in all four directions, with click-drag rectangular selection for applying a shape to many panels at once. Rounded edge radius accepts arbitrary values from 0.5 m upward with up to ten decimal places. Disabled cells are removed from geometry and signal linking so irregular wall silhouettes can be built. The editor tool also includes a live canvas preview, overflow validation, an NDI/Media Source picker, Rectangle RGB, Round RGB, and Round Linear physical subpixel simulations, and buttons to create a new wall or update the selected one. In the standalone switcher, **Refresh Inputs** discovers visible NDI senders and adds them directly to the crosspoint list; manual URLs remain available as a fallback.

## GDTF and DMX fixture builder

Open **Tools > TSAV GDTF DMX Fixture Builder** to import a `.gdtf` definition. When the GDTF contains glTF/GLB model resources, the tool extracts them from the archive, imports the referenced meshes, assigns the base, yoke, head, and lens, scales each part to its declared GDTF dimensions, and assembles it from the GDTF geometry transforms. FBX, OBJ, glTF, and GLB can still be imported manually when the GDTF has no compatible embedded model. The tool creates the Unreal DMX fixture type, library, patch, and a functioning fixture actor with configurable model scale and rotation, pan/tilt pivots and limits, movement speed, beam orientation, zoom, color, dimmer, and live preview values.

See `Plugins/TSAVLEDTools/README.md` for the short workflow.

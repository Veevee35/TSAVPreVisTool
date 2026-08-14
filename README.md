# TSAV PreVis Tool

Unreal Engine tools for A/V production previsualization.

## LED and NDI builder

The project includes reusable **TSAV LED Panel Definition**, **TSAV LED Panel**, and **TSAV LED Wall Builder** types in the `TSAVLEDTools` plugin. Open **Tools > TSAV LED Wall Builder** for the guided panel, wall, canvas, and NDI workflow. Open `/Game/TSAV/Levels/LED_Canvas_Configurator` for a ready-made level containing a center LED wall, two standalone panels, lighting, a floor, and a preview camera.

Panel definitions store real-world cabinet dimensions and native pixel resolution. The wall links that cabinet into rows and columns, calculates total screen resolution, generates serpentine cabinet topology records, and maps the resulting screen to an exact X/Y rectangle on a 4096×2160 processor canvas. Every column seam and row seam can bend from -90° to +90° in 0.5° steps, including simultaneous two-axis curvature and 90° under-folds for isolated enabled runs. Each column can also enable two independent signed circular radii inside every panel, producing convex, concave, or mixed S-curves while its seam corners remain connected. Per-row flat overrides ignore those internal column curves without disabling row-seam folds; adjacent curved panels taper to the shared straight hinge so top and bottom surfaces stay flat, watertight, and attached. Bends use shared video-plane corners so the front image remains watertight, with cabinet clearance and collision behind it; every cabinet side and exposed outer edge retreats inward and backward at 45° to avoid clipping tightly folded panels. An interactive paint grid supports square, disabled, full corner-to-corner diagonal, and corner-spanning rounded cabinets in all four directions. Rounded edge radius accepts arbitrary values from 0.5 m upward with up to ten decimal places. Disabled cells are removed from geometry and signal linking so irregular wall silhouettes can be built. The editor tool also includes a live canvas preview, overflow validation, an NDI/Media Source picker, Rectangle RGB and Round RGB physical subpixel simulations, and buttons to create a new wall or update the selected one.

## GDTF and DMX fixture builder

Open **Tools > TSAV GDTF DMX Fixture Builder** to import a `.gdtf` definition. When the GDTF contains glTF/GLB model resources, the tool extracts them from the archive, imports the referenced meshes, assigns the base, yoke, head, and lens, scales each part to its declared GDTF dimensions, and assembles it from the GDTF geometry transforms. FBX, OBJ, glTF, and GLB can still be imported manually when the GDTF has no compatible embedded model. The tool creates the Unreal DMX fixture type, library, patch, and a functioning fixture actor with configurable model scale and rotation, pan/tilt pivots and limits, movement speed, beam orientation, zoom, color, dimmer, and live preview values.

See `Plugins/TSAVLEDTools/README.md` for the short workflow.

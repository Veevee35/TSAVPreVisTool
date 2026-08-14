# TSAV PreVis Tool

Unreal Engine tools for A/V production previsualization.

## LED and NDI builder

The project includes reusable **TSAV LED Panel Definition**, **TSAV LED Panel**, and **TSAV LED Wall Builder** types in the `TSAVLEDTools` plugin. Open **Tools > TSAV LED Wall Builder** for the guided panel, wall, canvas, and NDI workflow. Open `/Game/TSAV/Levels/LED_Canvas_Configurator` for a ready-made level containing a center LED wall, two standalone panels, lighting, a floor, and a preview camera.

Panel definitions store real-world cabinet dimensions and native pixel resolution. The wall links that cabinet into rows and columns, calculates total screen resolution, generates serpentine cabinet topology records, and maps the resulting screen to an exact X/Y rectangle on a 4096×2160 processor canvas. Each seam between columns can bend from -15° to +15° in 0.5° steps to form a connected arc. Bends hinge at the video plane so the front image remains watertight, with cabinet clearance and collision behind it. An interactive paint grid supports square, disabled, full corner-to-corner diagonal, and rounded corner cabinets in all four corner directions. Disabled cells are removed from geometry and signal linking so irregular wall silhouettes can be built. The editor tool also includes a live canvas preview, overflow validation, an NDI/Media Source picker, Rectangle RGB and Round RGB physical subpixel simulations, and buttons to create a new wall or update the selected one.

## GDTF and DMX fixture builder

Open **Tools > TSAV GDTF DMX Fixture Builder** to import a `.gdtf` definition. When the GDTF contains glTF/GLB model resources, the tool extracts them from the archive, imports the referenced meshes, assigns the base, yoke, head, and lens, scales each part to its declared GDTF dimensions, and assembles it from the GDTF geometry transforms. FBX, OBJ, glTF, and GLB can still be imported manually when the GDTF has no compatible embedded model. The tool creates the Unreal DMX fixture type, library, patch, and a functioning fixture actor with configurable model scale and rotation, pan/tilt pivots and limits, movement speed, beam orientation, zoom, color, dimmer, and live preview values.

See `Plugins/TSAVLEDTools/README.md` for the short workflow.

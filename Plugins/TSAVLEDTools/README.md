# TSAV LED Tools

This plugin provides a guided editor builder, a cabinet definition, and two placeable actors:

- **TSAV LED Wall Builder tab** — an operator-facing workflow under **Tools > TSAV LED Wall Builder** with a visual processor-canvas preview and create/update actions.

- **TSAV LED Panel Definition** — a reusable cabinet model containing physical width, height, depth, bezel, and native X/Y pixel resolution.
- **TSAV LED Panel** — one custom cabinet that can use a definition asset or instance-specific physical and pixel dimensions.
- **TSAV LED Wall Builder** — a parametric wall that links one cabinet definition into rows and columns, generates cabinet signal order, and calculates the screen's native resolution.
- **TSAV GDTF DMX Fixture Builder** — imports a GDTF definition, automatically extracts and assigns its embedded glTF/GLB model when present, creates a DMX library/type/patch, and places an articulated, DMX-driven fixture.

Both actors expose a **Media Source** field under **TSAV LED > Media**. Assign an NDI Media Source asset there to route that feed to the surface. Under **TSAV LED > Canvas**, set the processor canvas size and the top-left pixel coordinate for that screen. The bundled material crops the Media/NDI texture to that exact rectangle.

For example, an 8×4 wall made from 128×128 cabinets has a native resolution of 1024×512. With a 4096×2160 canvas and a Canvas Position of X=512, Y=256, the wall samples pixels `(512,256)` through `(1535,767)` from the incoming source.

## Guided workflow

1. Restart Unreal Editor after the plugin is first compiled, then open **Tools > TSAV LED Wall Builder**.
2. Pick a saved **TSAV LED Panel Definition**, or clear the picker and enter a panel's width, height, depth, and native X/Y pixels. Custom values can be saved as a new preset.
3. Enter Columns and Rows. Set each seam between neighboring columns from -90° to +90° in 0.5° steps; repeated bends form a connected arc. Every bend hinges on the video plane, keeping the front image watertight while **Rear Cabinet Gap** reserves clearance only behind it. Choose **Square**, **Empty**, one of four full corner-to-corner diagonal halves, or one of four rounded corners, then click panels in the shape grid to paint that state; right-click restores a square panel. Empty cells generate no cabinet geometry or signal link, and the outer trim follows the remaining wall silhouette. Media UVs retain their original full-grid positions so irregular walls remain predictable on the processor canvas. The tool also calculates the active cabinet count, wall size, total grid resolution, and pixel pitch. Choose **Off**, **Rectangle RGB**, or **Round RGB** under Subpixel Layout. Strength blends from solid video at 0% to the full emitter mask at 100%.
4. Keep the processor canvas at 4096×2160 (or enter another size), then set the screen's top-left X/Y coordinate. The preview turns red and disables creation if the screen is outside the canvas.
5. Pick an **NDI Media Source** in **NDI / Media Source** and enable editor preview when desired.
6. Click **Create LED Wall** to place it in front of the active viewport. To edit an existing wall, select it, click **Load Selected Wall**, change the configuration, and click **Update Selected Wall**.

## Example level

1. Open `Content/TSAV/Levels/LED_Canvas_Configurator`.
2. Duplicate `DA_TSAV_500mm_128px` under the plugin's `PanelDefinitions` folder to add a cabinet model, then edit its size and native resolution.
3. Select the center wall, assign the panel definition, and set Rows/Columns.
4. Set **Canvas Resolution** (4096×2160 by default) and **Canvas Position**.
5. Expand **Generated Panel Links** to inspect each cabinet's serpentine link number, exact canvas coordinate, column angle, and edge style.
6. Assign an NDI Media Source, enable **Play in Editor**, and click **Refresh Media**; or press Play to open the feed automatically.

The bundled display material repeats the selected RGB emitter layout once per native wall pixel, after the incoming canvas has been cropped and sampled at native resolution. **Rectangle RGB** uses vertical red/green/blue stripe emitters; **Round RGB** uses a triangular cluster of round emitters. At a distance, texture filtering blends them back into the image; close up, the individual emitters and black mask are visible. **Off** skips the mask for a conventional solid-video preview.

The material exposes `MediaTexture`, `EmissiveStrength`, `CanvasScaleX/Y`, `CanvasOffsetX/Y`, `CanvasVisible`, `SurfaceResolutionX/Y`, `SubpixelTexture`, and `SubpixelStrength`. A custom material override should expose those same parameters.

## GDTF DMX fixture workflow

1. Enable Unreal's DMX Engine and GDTF support, restart the editor, then open **Tools > TSAV GDTF DMX Fixture Builder**.
2. Choose the `.gdtf` file and select its operating mode.
3. If the GDTF contains glTF/GLB resources, confirm the automatically imported Base, Yoke, Head, and Lens assignments. Each embedded part is scaled to the dimensions declared by the GDTF and assembled from its geometry transforms. Otherwise import a fixture model (`.fbx`, `.obj`, `.gltf`, or `.glb`) manually. A single imported mesh can be used for a static fixture; separate meshes are recommended for moving heads. Legacy 3DS-only GDTFs require conversion to glTF/GLB first.
4. Review the GDTF-derived pivot positions, motion limits/speeds, lens transform, beam direction, zoom range, and maximum intensity; adjust them only when the source data needs correction.
5. Set the DMX universe and address, then click **Create Fixture**. The tool creates the DMX Library, Fixture Type, Fixture Patch, and linked actor.
6. Use the preview controls to check pan/tilt and beam alignment. Select the fixture later and click **Load Selected Fixture** to adjust or repatch it.

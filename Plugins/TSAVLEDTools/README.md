# TSAV LED Tools

This plugin provides a guided editor builder, a cabinet definition, and two placeable actors:

- **TSAV LED Wall Builder tab** — an operator-facing workflow under **Tools > TSAV LED Wall Builder** with a visual processor-canvas preview and create/update actions.

- **TSAV LED Panel Definition** — a reusable cabinet model containing physical width, height, depth, bezel, and native X/Y pixel resolution.
- **TSAV LED Panel** — one custom cabinet that can use a definition asset or instance-specific physical and pixel dimensions.
- **TSAV LED Wall Builder** — a parametric wall that links one cabinet definition into rows and columns, generates cabinet signal order, and calculates the screen's native resolution.

Both actors expose a **Media Source** field under **TSAV LED > Media**. Assign an NDI Media Source asset there to route that feed to the surface. Under **TSAV LED > Canvas**, set the processor canvas size and the top-left pixel coordinate for that screen. The bundled material crops the Media/NDI texture to that exact rectangle.

For example, an 8×4 wall made from 128×128 cabinets has a native resolution of 1024×512. With a 4096×2160 canvas and a Canvas Position of X=512, Y=256, the wall samples pixels `(512,256)` through `(1535,767)` from the incoming source.

## Guided workflow

1. Restart Unreal Editor after the plugin is first compiled, then open **Tools > TSAV LED Wall Builder**.
2. Pick a saved **TSAV LED Panel Definition**, or clear the picker and enter a panel's width, height, depth, and native X/Y pixels. Custom values can be saved as a new preset.
3. Enter Columns and Rows. The tool calculates the cabinet count, wall size, total screen resolution, and pixel pitch.
4. Keep the processor canvas at 4096×2160 (or enter another size), then set the screen's top-left X/Y coordinate. The preview turns red and disables creation if the screen is outside the canvas.
5. Pick an **NDI Media Source** in **NDI / Media Source** and enable editor preview when desired.
6. Click **Create LED Wall** to place it in front of the active viewport. To edit an existing wall, select it, click **Load Selected Wall**, change the configuration, and click **Update Selected Wall**.

## Example level

1. Open `Content/TSAV/Levels/LED_Canvas_Configurator`.
2. Duplicate `DA_TSAV_500mm_128px` under the plugin's `PanelDefinitions` folder to add a cabinet model, then edit its size and native resolution.
3. Select the center wall, assign the panel definition, and set Rows/Columns.
4. Set **Canvas Resolution** (4096×2160 by default) and **Canvas Position**.
5. Expand **Generated Panel Links** to inspect each cabinet's serpentine link number and exact canvas coordinate.
6. Assign an NDI Media Source, enable **Play in Editor**, and click **Refresh Media**; or press Play to open the feed automatically.

The bundled display material exposes `MediaTexture`, `EmissiveStrength`, `CanvasScaleX/Y`, `CanvasOffsetX/Y`, and `CanvasVisible`. A custom material override should expose those same parameters.

# TSAV PreVis Tool

Unreal Engine tools for A/V production previsualization.

## LED and NDI builder

The project includes reusable **TSAV LED Panel Definition**, **TSAV LED Panel**, and **TSAV LED Wall Builder** types in the `TSAVLEDTools` plugin. Open **Tools > TSAV LED Wall Builder** for the guided panel, wall, canvas, and NDI workflow. Open `/Game/TSAV/Levels/LED_Canvas_Configurator` for a ready-made level containing a center LED wall, two standalone panels, lighting, a floor, and a preview camera.

Panel definitions store real-world cabinet dimensions and native pixel resolution. The wall links that cabinet into rows and columns, calculates total screen resolution, generates serpentine cabinet topology records, and maps the resulting screen to an exact X/Y rectangle on a 4096×2160 processor canvas. The editor tool includes a live canvas preview, overflow validation, an NDI/Media Source picker, and buttons to create a new wall or update the selected one.

See `Plugins/TSAVLEDTools/README.md` for the short workflow.

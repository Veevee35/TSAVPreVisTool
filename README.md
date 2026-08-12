# TSAV PreVis Tool

Unreal Engine tools for A/V production previsualization.

## LED and NDI builder

The project includes reusable **TSAV LED Panel** and **TSAV LED Wall Builder** actors in the `TSAVLEDTools` runtime plugin. Open `/Game/TSAV/Levels/LED_NDI_Builder` for a ready-made level containing a center LED wall, two standalone panels, lighting, a floor, and a preview camera.

Select an LED actor and assign any Unreal Media Source—including an **NDI Media Source**—to **TSAV LED > Media > Media Source**. Wall dimensions, cabinet count, gaps, borders, editor preview, looping, and emissive brightness are all editable from the Details panel.

See `Plugins/TSAVLEDTools/README.md` for the short workflow.

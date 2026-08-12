# TSAV LED Tools

This runtime plugin provides two placeable actors:

- **TSAV LED Panel** — one dimensionally editable LED cabinet.
- **TSAV LED Wall Builder** — a parametric wall with rows, columns, cabinet sizing, gaps, border, backing, and generated seams.

Both actors expose a **Media Source** field under **TSAV LED > Media**. Assign an NDI Media Source asset there to route that feed to the surface. The wall uses one continuous video surface, so the image does not repeat once per cabinet.

## Quick use

1. Open `Content/TSAV/Levels/LED_NDI_Builder`.
2. Select the center wall or either side panel.
3. Assign an NDI Media Source to **Media Source**.
4. Enable **Play in Editor** and click **Refresh Media**, or press Play to open the feed automatically.
5. Edit panel dimensions or the wall's row/column settings directly in the Details panel.

The bundled display material exposes `MediaTexture` and `EmissiveStrength`. A custom material override can be used if it exposes parameters with those same names.

# Asset inventory

Inventory date: 2026-08-15. Binary Unreal assets were queried through the UE 5.8 MCP asset registry and scene/object tools. Filesystem enumeration was used only for counts and paths; `.uasset` and `.umap` contents were not parsed or edited directly.

## Totals

| Scope | Count |
|---|---:|
| Project `.uasset` files | 2,102 |
| Project `.umap` files before Phase 1 | 4 |
| New TSAV application map | 1 |
| Current project Unreal assets | 2,107 |
| `TSAVLEDTools` plugin content assets | 8 |

Project content is dominated by imported venue and fixture libraries:

| Content root | Assets |
|---|---:|
| `SketchUp` | 1,298 |
| `DMX` | 610 |
| `TSAV` before the app map | 97 |
| `VprodProject` | 25 |
| `Showcase` | 15 |
| Root-level media/NDI/material assets and `Screens` | 61 |

`/Game/TSAV` now contains 95 fixture assets, two legacy TSAV levels, and the new application map.

## Level inventory verified through MCP

| Level | Actors | Key verified actor classes |
|---|---:|---|
| `/Game/VprodProject/Maps/Main` | 20 | 1 `ATSAVLEDWall`, 4 Cine Cameras, 2 Static Mesh Actors, PlayerStart, sky/fog/cloud lighting |
| `/Game/TSAV/Levels/LED_NDI_Builder` | 14 | 1 `ATSAVLEDWall`, 2 `ATSAVLEDPanel`, CameraActor, floor/light actors |
| `/Game/TSAV/Levels/LED_Canvas_Configurator` | 10 | Clean floor/light shell; no TSAV runtime actors currently placed |
| `/Game/Showcase/TSAV_Feature_Showcase` | 88 | 44 Static Mesh Actors, 17 DMX fixture Blueprints, 10 lights, 2 Cine Cameras, 5 Text Render Actors |
| `/Game/TSAV/App/L_TSAV_App` | 10 | Clean floor/light shell copied through MCP; WorldSettings uses `ATSAVAppGameMode` |

The editor still opens `Main`; packaged launches use `L_TSAV_App`. `Content/VprodProject/Maps/Main.umap` was already modified in the user's working tree and was left untouched.

## TSAV fixture inventory

The `/Game/TSAV/Fixtures` registry contains:

- 7 generated DMX fixture assets under `Fixtures/DMX`;
- 14 GDTF import assets under `Fixtures/GDTF`;
- imported Static Mesh, Skeletal Mesh, material, skeleton, animation, and physics assets under `Fixtures/Models`.

The larger `/Game/DMX/GDTF_Fixtures` library contains hundreds of content-only GDTF assets and accounts for most of the 610 assets under `DMX`.

## TSAV LED plugin content

| Asset | Registry path |
|---|---|
| Canvas frame material | `/TSAVLEDTools/Materials/M_TSAV_LEDCanvasFrame` |
| Canvas video material | `/TSAVLEDTools/Materials/M_TSAV_LEDCanvasVideo` |
| Frame material | `/TSAVLEDTools/Materials/M_TSAV_LEDFrame` |
| Video material | `/TSAVLEDTools/Materials/M_TSAV_LEDVideo` |
| Rectangle RGB texture | `/TSAVLEDTools/Subpixels/T_TSAV_Subpixel_RectangleRGB` |
| Round RGB texture | `/TSAVLEDTools/Subpixels/T_TSAV_Subpixel_RoundRGB` |
| 500 mm panel definition | `/TSAVLEDTools/PanelDefinitions/DA_TSAV_500mm_128px` |
| Duplicate panel definition | `/TSAVLEDTools/PanelDefinitions/DA_TSAV_500mm_128px1` |

## Root-level media assets

The root content directory contains the existing CAM 1–4 NDI sources, center/side LED media sources, players, playlists, NDI sources, material instances, projector media assets, DMX control consoles, and DMX library. Their exact runtime routing relationships are not inferred from filenames; they should be inspected individually through MCP when the video-router migration begins.

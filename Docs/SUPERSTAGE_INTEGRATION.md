# SuperStage in TSAV PreVis

The current development direction is independent TSAV functionality. Use `Start-TSAVPreVis.cmd` for native tools with SuperStage disabled; see [native feature coverage](TSAV_NATIVE_FEATURES.md). The instructions below describe the optional vendor integration, which retains its vendor account requirements.

## Start the integrated editor

Double-click `Start-TSAVSuperStage.cmd` in the project root. In Unreal, open **Tools > TSAV SuperStage** for the installed vendor panels, lighting library, and content browser shortcuts. The original SuperStage toolbar and Window menu remain the entry points for its complete workflows. Sign in through the vendor's own panel to activate the features included in your account.

For a different engine location or to compile TSAV before opening it:

```powershell
.\Build\Start-TSAVSuperStage.ps1 -UnrealRoot 'C:\UE_5.8' -Build
```

Close any existing editor for this project first. SuperStage's shader module must load during engine startup; it cannot be added safely to an editor session that is already running.

## Patch lights from the TSAV Lighting Console

Open **Tools > TSAV Lighting Console** in the integrated editor. Placed SuperStage fixtures appear alongside the TSAV catalog, with their fixture ID, universe, starting channel, and complete channel footprint. **Show placed fixtures only** and **Select Placed** help work with the current scene.

1. Check the fixtures to patch. The last individually checked fixture becomes the primary fixture, whose current address and mode attributes appear on the right.
2. Enter **Universe** and **Start address**, then click **Patch selected in list order**. A batch follows the displayed list order, uses consecutive channels, and rolls subsequent fixtures into the next universe when necessary. The first fixture must fit at the address you entered.
3. The entire batch is checked for overlaps and valid channel footprints before applying it. SuperStage supports internal universes 1–512. An unplaced TSAV catalog template does not reserve space against a batch consisting entirely of SuperStage actors; placed TSAV actors do.
4. TSAV writes the SuperStage actor's actual `SuperDMXFixture` property and synchronizes its imported console patch. **Sync scene patch with SuperStage console** explicitly scans and imports the scene if it has not been imported yet. Existing imported records retain their console IDs.
5. **Save All** persists actor, catalog, and library changes. Patch edits support Unreal Undo. Scene patch changes made with the vendor tools appear in TSAV while this panel is open, including after Undo.

Both TSAV scene rows and SuperStage rows represent individual placed actors. Patching a TSAV scene row creates a separate native DMX patch; a TSAV catalog row edits its shared library template. Selecting both kinds of actors does not convert one into the other.

The programmer uses the native DMX library for TSAV rows and the vendor console's reflected DMX API for SuperStage rows. SuperStage's internal console ID is resolved from the actor GUID; the scene's displayed FixtureID is a different identifier. Unimported or stale vendor patches are rejected before sending. Enable **SuperStage console output enabled** to use the vendor output path. Raw dimmer faders respect TSAV's grand master and blackout. Rebuilding or changing selection retains all mode faders.

Native TSAV output universes must also be covered by a DMX output port in **Project Settings > DMX**. The patch action reports a missing port range. SuperStage network configuration is separate; its input/output protocol, adapter, and universe offsets are controlled by the vendor's **SuperDMX** panel. See the [vendor network configuration](https://yunsio.com/docs/stagecore/dmx-network-configuration).

### Activation and the first light check

This workstation has not yet been signed into and activated through SuperStage. The shared patch and console-value checks work, but fixture output has not been established. The isolated fixture probe reported zero registered/evaluated fixtures and no raw channel readback. That observation alone does not identify activation as the cause.

1. Start with `Start-TSAVSuperStage.cmd`, open the vendor panel, and sign in to activate the account's license or trial. Activation is handled by the vendor account, as described in its [installation guide](https://yunsio.com/docs/get-started/installation).
2. Place a SuperStage fixture definition, select a valid mode, and set its control mode to **DMX**. Select that placed row in TSAV, set its universe/address, and apply the patch.
3. Enable **SuperStage console output enabled**. In SuperConsole's settings, confirm the same fixture/address on the Patch page, then test its dimmer. The vendor's [console documentation](https://yunsio.com/docs/console) distinguishes console output, which drives the internal fixture buffer, from network output, which is only needed for external devices.
4. Once the vendor console controls the light, test the same fixture from TSAV and confirm dimmer, blackout, and address changes in the viewport. This visual acceptance check remains outstanding; a successful patch or programmer-value update does not confirm light output.

## Installed distribution

The complete `SuperStage_UE58_26H2.6.rar` distribution is installed at `Plugins/SuperStage`, retaining its original plugin name, binaries, shaders, content paths, configuration, resources, notices, and Intermediate files. No vendor code, activation controls, or assets were rewritten.

| Item | Verified value |
|---|---|
| Release | 26H2.6 |
| Engine | UE 5.8.1, CL 56057345 |
| Plugin and engine binary build ID | 55116800 |
| Extracted files | 12,564 |
| Extracted bytes | 6,615,879,026 |
| Unreal assets | 9,586 `.uasset` files |
| Fixture profiles | 904 `.gdtf` files |
| Archive SHA-256 | `47DF83CF48CF0AE38B6380EAFB1EAE37DF517E1ED7D1FD079738481B0A23B0FF` |

| Module | Supplied role |
|---|---|
| SuperCore | Fixture, beam, lighting, and shared scene types |
| SuperAssets | Stage floors, truss, scaffolding, barriers, drapes, crowd, screens, machinery, and effects |
| SuperDMX | Vendor fixture patching and DMX |
| SuperNdi | Vendor NDI/media support with bundled NDI DLLs |
| SuperMadrix | Vendor MADRIX integration module |
| SuperLaser | Vendor laser support |
| SuperShader | Shader integration, loaded at PostConfigInit |
| SuperAuth | Vendor account and feature authorization |
| SuperTools | Editor tools |
| SuperConsole | Editor console |

Presence of a module does not establish account entitlement or full feature functionality. The TSAV Lighting Console includes the editor patch bridge described above. SuperStage tools operate on their own actor types; TSAV switcher routing and `.tsav` persistence have not been adapted to arbitrary SuperStage actors.

## Why this uses a launcher

This archive contains **only UnrealEditor binaries**. It has no `Source` directory, `.Build.cs` definitions, public SDK headers, game target libraries, or non-editor precompiled manifests. The descriptor marks eight modules Runtime, but those labels do not supply the missing standalone binaries.

Passing this plugin to UnrealBuildTool with `-EnablePlugin=SuperStage` fails with:

```text
Could not find definition for module 'SuperCore',
(referenced via LiveEventTestEditor EnablePlugins -> SuperStage.uplugin)
```

Consequently the project descriptor keeps SuperStage disabled for normal compilation. The launcher checks the installed binaries against the engine, then supplies the **editor process** argument `-EnablePlugins=SuperStage`. The TSAV editor bridge discovers vendor tab spawners using Unreal's public API and has no link dependency on vendor modules. Normal double-clicking of `LiveEventTest.uproject` opens the standard TSAV editor without SuperStage; use the supplied launcher for the combined toolset. Do not persistently enable SuperStage in the Plugins dialog with this binary-only archive, because subsequent C++ builds would require its missing build definitions.

`Build/Package-TSAVPreVis.ps1` explicitly disables SuperStage in the cooker. **The standalone executable does not include SuperStage.** Completing that integration requires a vendor-supported UE 5.8 runtime SDK/source distribution with build rules, headers, non-editor libraries, and the applicable deployment entitlement. The vendor's console/tools are Editor modules and cannot become packaged application UI merely by changing descriptor flags.

## Restore on another licensed workstation

The vendor directory is local and ignored by Git; the TSAV integration code and scripts are versioned. Keep the original vendor archive available for reinstalling.

```powershell
.\Build\Install-SuperStage.ps1 -ArchivePath 'C:\Downloads\SuperStage_UE58_26H2.6.rar'
.\Build\Test-SuperStage.ps1
.\Build\Start-TSAVSuperStage.ps1 -Build
```

The installer rejects unsafe archive paths/links, verifies extraction with 7-Zip's CRC checks, checks every extracted file's size, verifies the binary build ID, and preserves an existing installation. `Test-SuperStage.ps1` checks the descriptor, all ten DLLs, supporting directories, and engine compatibility. This check does not sign in or determine account entitlement.

## Validation

- Complete archive extraction passed 7-Zip CRC checks; all 12,564 installed file sizes matched the archive inventory.
- TSAV editor compiled with the integration enabled in its editor module.
- The Win64 Development standalone target also compiled successfully, with SuperStage excluded.
- All ten SuperStage DLLs loaded in the hidden editor smoke run, and the bundled NDI DLL initialized.
- `TSAV.SuperStage.EditorIntegration` checks module loading, representative actor registration, TSAV menu registration, native panel discovery, and the mounted content library. It confirms all 9,586 registered assets and finds the VAT Generator panel. The TSAV submenu mirrors only panels exposed to Unreal's public menu-spawner list.
- `TSAV.LightingConsole.PatchPlan` checks consecutive patching, universe rollover, exact starting address, overlap rejection, channel footprints, and universe limits.
- `TSAV.SuperStage.SharedPatch` checks actual vendor actor patching, imported console records, internal console ID resolution, Undo, external address refresh, native/vendor raw fader rebuilding, output enable/disable, and grand master/blackout values at the vendor console API. These checks exercise a temporary actor in the unattended Entry map and do not save the user's scene or repack the native catalog.
- `TSAV.SuperStage.FixtureOutputProbe` is a separate, optional diagnostic that requires a working vendor session. It checks fixture registration/evaluation and raw dimmer/blackout readback. It currently fails on this unactivated workstation. A component's authored intensity is not used as proof of rendered output. Passing this probe would still require the viewport acceptance check above.
- Latest combined run: the three integration/patch tests passed; the fixture-output probe failed (zero registered/evaluated fixtures, raw readback `-1`). Report: `Saved/SuperStageReview/Run-60cfe17184f0404fa3470547e9b0c923/index.json`.
- Added the missing `GameFeatureData` Asset Manager rule required by the enabled editor plugin stack; the subsequent run had no logged startup errors.
- No external DMX console, NDI sender, laser hardware, or vendor account entitlement is exercised by these checks. A NullRHI load check does not validate rendered lighting/effects.

Run the three integration and patch tests after building the editor:

```powershell
.\Build\Test-SuperStage.ps1 -EditorSmoke
```

Repeat the additional fixture diagnostic after activation:

```powershell
.\Build\Test-SuperStage.ps1 -EditorSmoke -FixtureOutputProbe -RenderSmoke
```

The smoke process uses a temporary map and its own vendor network defaults, with network output disabled and a loopback-only destination. The script prints explicitly when fixture output has not been checked. It never signs in or changes account entitlement.

The supplied vendor assets log warnings for the missing `Super/Projector/LT_Maping` texture and legacy `EInfiniteRotationMode::Stop` values in Aurora Fan assets. The complete extraction was verified; these references originate in the supplied release. No vendor assets were rewritten to conceal them. External NDI senders that are offline also produce the existing TSAV media warnings.

Local run evidence is under `Saved/SuperStageReview` (ignored by Git).

## Vendor references

- [Installation and project-local plugin placement](https://yunsio.com/docs/get-started/installation)
- [Fixture library workflow](https://yunsio.com/docs/editortools/fixture-library-editor)
- [Feature licensing and C++ development access](https://yunsio.com/pricing)

The supplied license and third-party notices remain in the installed plugin directory for review.

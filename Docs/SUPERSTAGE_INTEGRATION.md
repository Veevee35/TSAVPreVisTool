# SuperStage in TSAV PreVis

## Start the integrated editor

Double-click `Start-TSAVSuperStage.cmd` in the project root. In Unreal, open **Tools > TSAV SuperStage** for the installed vendor panels, lighting library, and content browser shortcuts. The original SuperStage toolbar and Window menu remain the entry points for its complete workflows. Sign in through the vendor's own panel to activate the features included in your account.

For a different engine location or to compile TSAV before opening it:

```powershell
.\Build\Start-TSAVSuperStage.ps1 -UnrealRoot 'C:\UE_5.8' -Build
```

Close any existing editor for this project first. SuperStage's shader module must load during engine startup; it cannot be added safely to an editor session that is already running.

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

Presence of a module does not establish account entitlement or full feature functionality. The existing TSAV LED tools, cameras, switcher, DMX tools, and standalone application retain their implementations. SuperStage tools operate on their own actor types; TSAV switcher routing and `.tsav` persistence have not been adapted to arbitrary SuperStage actors.

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
- The automated Unreal test `TSAV.SuperStage.EditorIntegration` checks module loading, representative actor registration, TSAV menu registration, native panel discovery, and the mounted content library. Run with SuperStage enabled at editor startup.
- The test passed (1 success, 0 failures), confirmed all 9,586 registered assets, and found the VAT Generator panel. Other panels and licensed workflows remain available through the vendor UI as enabled by the account; they were not functionally tested. The TSAV submenu mirrors only panels the vendor exposes to Unreal's public menu-spawner list.
- Added the missing `GameFeatureData` Asset Manager rule required by the enabled editor plugin stack; the subsequent run had no logged startup errors.
- No external DMX console, NDI sender, laser hardware, or vendor account entitlement is exercised by these checks. A NullRHI load check does not validate rendered lighting/effects.

Run the same automated test after building the editor:

```powershell
.\Build\Test-SuperStage.ps1 -EditorSmoke
```

The supplied vendor assets log warnings for the missing `Super/Projector/LT_Maping` texture and legacy `EInfiniteRotationMode::Stop` values in Aurora Fan assets. The complete extraction was verified; these references originate in the supplied release. No vendor assets were rewritten to conceal them. External NDI senders that are offline also produce the existing TSAV media warnings.

Local run evidence is under `Saved/SuperStageReview` (ignored by Git).

## Vendor references

- [Installation and project-local plugin placement](https://yunsio.com/docs/get-started/installation)
- [Fixture library workflow](https://yunsio.com/docs/editortools/fixture-library-editor)
- [Feature licensing and C++ development access](https://yunsio.com/pricing)

The supplied license and third-party notices remain in the installed plugin directory for review.

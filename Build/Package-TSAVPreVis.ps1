param(
    [ValidateSet('Development', 'Shipping')]
    [string] $Configuration = 'Shipping',

    [string] $UnrealRoot = 'C:\UE_5.8',

    [string] $ArchiveDirectory
)

$ErrorActionPreference = 'Stop'

$TsavProjectRoot = Split-Path -Parent $PSScriptRoot
$TsavProjectFile = Join-Path $TsavProjectRoot 'LiveEventTest.uproject'
$TsavRunUat = Join-Path $UnrealRoot 'Engine\Build\BatchFiles\RunUAT.bat'

if (-not $ArchiveDirectory) {
    $ArchiveDirectory = Join-Path $TsavProjectRoot "Saved\Packages\$Configuration"
}

if (-not (Test-Path -LiteralPath $TsavRunUat)) {
    throw "RunUAT.bat was not found under UnrealRoot: $UnrealRoot"
}

# These plugins belong to the editor/Codex authoring target. Unreal's cooker is
# hosted by UnrealEditor, so explicitly disabling them prevents editor receipts
# from expanding a standalone cook. Runtime dependencies (DMX, NDI, media I/O,
# Enhanced Input, and procedural meshes) remain enabled through the game receipt.
$TsavEditorOnlyPlugins = @(
    'TSAVPrevisTools',
    'OpenXR',
    'VirtualScouting',
    'HDRIBackdrop',
    'SunPosition',
    'VirtualProductionUtilities',
    'LiveLink',
    'Composite',
    'RemoteControl',
    'nDisplay',
    'AjaMedia',
    'BlackmagicMedia',
    'AppleProResMedia',
    'HAPMedia',
    'PixelStreaming',
    'MovieRenderPipeline',
    'DatasmithCADImporter',
    'DatasmithImporter',
    'DatasmithInterchange',
    'DatasmithMVR',
    'DatasmithFBXImporter',
    'DatasmithRuntime',
    'DaySequence',
    'DMXControlConsole',
    'DMXDisplayCluster',
    # DMXFixtures remains enabled: the generated catalog deliberately uses its
    # runtime base/yoke/head/lens meshes when a GDTF omits authored geometry.
    'DMXPixelMapping',
    'LiveLinkControlRig',
    'LiveLinkHub',
    'LiveLinkOverNDisplay',
    'LiveLinkCamera',
    'MetaHuman',
    'MetaHumanCalibrationProcessing',
    'MetaHumanCoreTech',
    'MetaHumanLiveLink',
    'MetaHumanCharacter',
    'MetaHumanCalibrationDiagnostics',
    'MetasoundExperimental',
    'MIDIDevice',
    'PanoramicCapture',
    'RemoteControlProtocolDMX',
    'RemoteControlProtocolMIDI',
    'RemoteControlProtocolOSC',
    'RemoteControlWebInterface',
    'RemoteDatabaseSupport',
    'WinDualShock',
    'ModelContextProtocol',
    'MCPClientToolset',
    'AllToolsets'
)

$TsavCookerOptions = '-DisablePlugins=' + ($TsavEditorOnlyPlugins -join ',')
$TsavUatArguments = @(
    'BuildCookRun',
    "-project=$TsavProjectFile",
    '-noP4',
    '-platform=Win64',
    "-clientconfig=$Configuration",
    '-build',
    '-cook',
    '-stage',
    '-pak',
    '-archive',
    "-archivedirectory=$ArchiveDirectory",
    '-map=/Game/TSAV/App/L_TSAV_App',
    "-AdditionalCookerOptions=$TsavCookerOptions",
    '-utf8output'
)

& $TsavRunUat @TsavUatArguments
if ($LASTEXITCODE -ne 0) {
    throw "TSAV PreVis packaging failed with exit code $LASTEXITCODE."
}

Write-Host "TSAV PreVis package created at: $ArchiveDirectory"

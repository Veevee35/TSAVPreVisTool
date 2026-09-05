param(
    [string] $ArchiveDirectory,
    [ValidateRange(15, 600)] [int] $TimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
$TsavRoot = Split-Path -Parent $PSScriptRoot
if (-not $ArchiveDirectory) { $ArchiveDirectory = Join-Path $TsavRoot 'Saved\Packages\NativeSuiteDevelopment' }
$TsavExe = Join-Path $ArchiveDirectory 'Windows\LiveEventTest\Binaries\Win64\LiveEventTest.exe'
if (-not (Test-Path -LiteralPath $TsavExe)) { throw "Build a Development package first. Executable not found: $TsavExe" }
$TsavReview = Join-Path $TsavRoot ('Saved\NativePackageReview\Run-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $TsavReview -Force | Out-Null
$TsavLog = Join-Path $TsavReview 'packaged-smoke.log'
# The existing runtime validation command operates on this fresh application's
# scene, saves its own Automation project and exits. It never loads a user's file.
$TsavArgs = @('-Unattended', '-NullRHI', '-NoSound', '-NoSplash', '-windowed',
    '-ResX=1280', '-ResY=720', '-ExecCmds=tsav.ValidatePhase2AndQuit',
    ('-abslog="' + $TsavLog + '"'), ('-UserDir="' + $TsavReview + '"'))
$TsavProcess = Start-Process -FilePath $TsavExe -ArgumentList $TsavArgs -WorkingDirectory (Split-Path $TsavExe) -WindowStyle Hidden -PassThru
if (-not $TsavProcess.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $TsavProcess.Id
    throw "Packaged smoke test timed out. See $TsavLog"
}
$TsavText = Get-Content -LiteralPath $TsavLog -Raw
foreach ($TsavMarker in @('CODEX_TSAV_PHASE2_COMMAND_PERSISTENCE_SUCCESS', 'CODEX_TSAV_VIDEO_SWITCHER_CAMERA_VISCA_SUCCESS',
    'CODEX_TSAV_LED_RUNTIME_CONFIGURATOR_VIDEO_SUCCESS', 'CODEX_TSAV_LED_FULLSCREEN_CONFIGURATOR_SUCCESS')) {
    if (-not $TsavText.Contains($TsavMarker)) { throw "Missing validation result $TsavMarker. See $TsavLog" }
}
if ($TsavProcess.ExitCode -ne 0 -or $TsavText.Contains('CODEX_TSAV_PHASE2_VALIDATION_FAILURE')) { throw "Packaged smoke test failed. See $TsavLog" }
Write-Host "Packaged TSAV project, undo, camera and LED routing smoke checks passed. Log: $TsavLog"

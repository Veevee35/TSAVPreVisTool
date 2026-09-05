param(
    [string] $UnrealRoot = 'C:\UE_5.8',
    [switch] $Build,
    [switch] $ValidateOnly
)
$ErrorActionPreference = 'Stop'
$TsavRoot = Split-Path -Parent $PSScriptRoot
$TsavProject = Join-Path $TsavRoot 'LiveEventTest.uproject'
$TsavStatus = & (Join-Path $PSScriptRoot 'Test-SuperStage.ps1') -UnrealRoot $UnrealRoot -AsObject
if ($ValidateOnly) { $TsavStatus | Format-List; return }

if ($Build) {
    # Compile TSAV normally. The binary-only vendor plugin cannot be passed to UBT.
    & (Join-Path $UnrealRoot 'Engine\Build\BatchFiles\Build.bat') LiveEventTestEditor Win64 Development "-Project=$TsavProject" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw "TSAV editor build failed ($LASTEXITCODE)." }
}
$TsavEditor = Join-Path $UnrealRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $TsavEditor)) { throw "UnrealEditor.exe not found: $TsavEditor" }
if (-not (Test-Path -LiteralPath (Join-Path $TsavRoot 'Binaries\Win64\UnrealEditor-LiveEventTest.dll'))) {
    throw 'Build the TSAV editor first: run this script with -Build.'
}
# SuperShader must load at PostConfigInit, before project modules and materials.
# Enable at process startup; loading it from an already-open editor is too late.
$TsavLaunchArgs = @(('"' + $TsavProject + '"'), '-EnablePlugins=SuperStage')
$TsavProcess = Start-Process -FilePath $TsavEditor -ArgumentList $TsavLaunchArgs -WorkingDirectory $TsavRoot -WindowStyle Hidden -PassThru
Write-Host "TSAV + SuperStage $($TsavStatus.Version) launched (PID $($TsavProcess.Id)). Open Tools > TSAV SuperStage or the SuperStage toolbar."

param(
    [string] $UnrealRoot = 'C:\UE_5.8',
    [string] $Map = '/Game/TSAV/App/L_TSAV_App',
    [switch] $Build
)
$ErrorActionPreference = 'Stop'
$TsavRoot = Split-Path -Parent $PSScriptRoot
$TsavProject = Join-Path $TsavRoot 'LiveEventTest.uproject'
if ($Build) {
    & (Join-Path $UnrealRoot 'Engine\Build\BatchFiles\Build.bat') LiveEventTestEditor Win64 Development "-Project=$TsavProject" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw "TSAV editor build failed ($LASTEXITCODE)." }
}
$TsavEditor = Join-Path $UnrealRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $TsavEditor)) { throw "UnrealEditor.exe not found: $TsavEditor" }
$TsavLaunchArgs = @(('"' + $TsavProject + '"'), ('"' + $Map + '"'), '-DisablePlugins=SuperStage')
$TsavProcess = Start-Process -FilePath $TsavEditor -ArgumentList $TsavLaunchArgs -WorkingDirectory $TsavRoot -WindowStyle Hidden -PassThru
Write-Host "TSAV PreVis launched at $Map with SuperStage disabled (PID $($TsavProcess.Id)). Open Tools > TSAV Lighting Console."

param(
    [string] $UnrealRoot = 'C:\UE_5.8',
    [ValidateRange(30, 1800)] [int] $TimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
$TsavRoot = Split-Path -Parent $PSScriptRoot
$TsavRunPath = Join-Path $TsavRoot ('Saved\NativeLightingReview\Run-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $TsavRunPath -Force | Out-Null
$TsavProject = Join-Path $TsavRoot 'LiveEventTest.uproject'
$TsavEditor = Join-Path $UnrealRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$TsavExpected = @('TSAV.LightingConsole.PatchPlan', 'TSAV.LightingConsole.NativeFixtures')
$TsavArgs = @(
    ('"' + $TsavProject + '"'), '/Engine/Maps/Entry',
    '-DisablePlugins=SuperStage', '-Unattended', '-NullRHI', '-NoSound', '-NoSplash',
    ('-EDITORPERPROJECTUSERSETTINGSINI="' + (Join-Path $TsavRunPath 'EditorPerProjectUserSettings.ini') + '"'),
    ('-ExecCmds="Automation RunTests ' + ($TsavExpected -join '+') + '"'),
    '-TestExit="Automation Test Queue Empty"',
    ('-ReportExportPath="' + $TsavRunPath + '"'),
    ('-abslog="' + (Join-Path $TsavRunPath 'editor.log') + '"')
)
$TsavProcess = Start-Process -FilePath $TsavEditor -ArgumentList $TsavArgs -WindowStyle Hidden -PassThru
if (-not $TsavProcess.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $TsavProcess.Id
    throw "Native lighting test timed out. See $TsavRunPath"
}
$TsavReportFile = Join-Path $TsavRunPath 'index.json'
if (-not (Test-Path -LiteralPath $TsavReportFile)) { throw "No automation report. See $TsavRunPath" }
$TsavReport = Get-Content -LiteralPath $TsavReportFile -Raw | ConvertFrom-Json
$TsavPassed = @($TsavReport.tests | Where-Object { $_.fullTestPath -in $TsavExpected -and $_.state -eq 'Success' })
if ($TsavProcess.ExitCode -ne 0 -or $TsavReport.failed -gt 0 -or $TsavPassed.Count -ne $TsavExpected.Count) {
    throw "Native lighting tests failed. See $TsavRunPath"
}
Write-Host "Native TSAV lighting tests passed with SuperStage disabled. Report: $TsavReportFile"

param(
    [string] $UnrealRoot = 'C:\UE_5.8',
    [switch] $AsObject,
    [switch] $EditorSmoke,
    [ValidateRange(30, 1800)]
    [int] $TimeoutSeconds = 300
)
$ErrorActionPreference = 'Stop'
$TsavRoot = Split-Path -Parent $PSScriptRoot
$TsavPluginRoot = Join-Path $TsavRoot 'Plugins\SuperStage'
$TsavDescriptorPath = Join-Path $TsavPluginRoot 'SuperStage.uplugin'
if (-not (Test-Path -LiteralPath $TsavDescriptorPath)) {
    throw 'SuperStage is missing. Run Build/Install-SuperStage.ps1 -ArchivePath <vendor archive>.'
}
$TsavDescriptor = Get-Content -LiteralPath $TsavDescriptorPath -Raw | ConvertFrom-Json
$TsavEngineManifest = Get-Content -LiteralPath (Join-Path $UnrealRoot 'Engine\Binaries\Win64\UnrealEditor.modules') -Raw | ConvertFrom-Json
$TsavPluginManifest = Get-Content -LiteralPath (Join-Path $TsavPluginRoot 'Binaries\Win64\UnrealEditor.modules') -Raw | ConvertFrom-Json
if ($TsavEngineManifest.BuildId -ne $TsavPluginManifest.BuildId) {
    throw "Engine/plugin build IDs differ: $($TsavEngineManifest.BuildId) / $($TsavPluginManifest.BuildId). Install a matching vendor release."
}
foreach ($TsavModule in $TsavDescriptor.Modules) {
    $TsavDll = $TsavPluginManifest.Modules.PSObject.Properties[$TsavModule.Name]
    if (-not $TsavDll -or -not (Test-Path -LiteralPath (Join-Path $TsavPluginRoot "Binaries\Win64\$($TsavDll.Value)"))) {
        throw "Missing SuperStage module binary: $($TsavModule.Name)"
    }
}
foreach ($TsavRequired in @('Content', 'Config', 'Resources', 'Shaders', 'FixtureLibrary\GDTF', 'LICENSE_en.txt', 'THIRD_PARTY_NOTICES.txt', 'Binaries\Win64\Processing.NDI.Lib.x64.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $TsavPluginRoot $TsavRequired))) {
        throw "Incomplete SuperStage installation: missing $TsavRequired"
    }
}
$TsavResult = [pscustomobject]@{
    Version = $TsavDescriptor.VersionName
    BuildId = $TsavPluginManifest.BuildId
    Modules = @($TsavDescriptor.Modules).Count
    PluginRoot = $TsavPluginRoot
    HasBuildRules = @(Get-ChildItem -LiteralPath $TsavPluginRoot -Filter '*.Build.cs' -Recurse).Count -gt 0
    Mode = 'Unreal Editor binary plugin; standalone runtime integration is not included'
}
if ($AsObject) { return $TsavResult }
$TsavResult | Format-List
if ($EditorSmoke) {
    $TsavRunPath = Join-Path $TsavRoot ('Saved\SuperStageReview\Run-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $TsavRunPath -Force | Out-Null
    $TsavProject = Join-Path $TsavRoot 'LiveEventTest.uproject'
    $TsavEditor = Join-Path $UnrealRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $TsavSmokeArgs = @(
        ('"' + $TsavProject + '"'),
        '-EnablePlugins=SuperStage', '-Unattended', '-NullRHI', '-NoSound', '-NoSplash',
        '-ExecCmds="Automation RunTests TSAV.SuperStage"',
        '-TestExit="Automation Test Queue Empty"',
        ('-ReportExportPath="' + $TsavRunPath + '"'),
        ('-abslog="' + (Join-Path $TsavRunPath 'editor.log') + '"')
    )
    $TsavSmokeProcess = Start-Process -FilePath $TsavEditor -ArgumentList $TsavSmokeArgs -WindowStyle Hidden -PassThru
    if (-not $TsavSmokeProcess.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $TsavSmokeProcess.Id
        throw "SuperStage smoke test timed out. See $TsavRunPath"
    }
    $TsavReportFile = Join-Path $TsavRunPath 'index.json'
    if (-not (Test-Path -LiteralPath $TsavReportFile)) { throw "No automation report produced. See $TsavRunPath" }
    $TsavReport = Get-Content -LiteralPath $TsavReportFile -Raw | ConvertFrom-Json
    $TsavIntegrationTest = @($TsavReport.tests | Where-Object { $_.fullTestPath -eq 'TSAV.SuperStage.EditorIntegration' })
    if ($TsavSmokeProcess.ExitCode -ne 0 -or $TsavReport.failed -gt 0 -or
        $TsavIntegrationTest.Count -ne 1 -or $TsavIntegrationTest[0].state -ne 'Success') {
        throw "SuperStage smoke test failed. See $TsavRunPath"
    }
    Write-Host "SuperStage module/menu/content smoke test passed. Report: $TsavReportFile"
}

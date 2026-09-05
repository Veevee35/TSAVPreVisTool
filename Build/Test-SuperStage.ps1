param(
    [string] $UnrealRoot = 'C:\UE_5.8',
    [switch] $AsObject,
    [switch] $EditorSmoke,
    [switch] $RenderSmoke,
    [switch] $FixtureOutputProbe,
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
    $TsavExpectedTests = @('TSAV.SuperStage.EditorIntegration', 'TSAV.SuperStage.SharedPatch', 'TSAV.LightingConsole.PatchPlan')
    if ($FixtureOutputProbe) { $TsavExpectedTests += 'TSAV.SuperStage.FixtureOutputProbe' }
    $TsavTestFilter = $TsavExpectedTests -join '+'
    $TsavRunPath = Join-Path $TsavRoot ('Saved\SuperStageReview\Run-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $TsavRunPath -Force | Out-Null
    $TsavNetworkIni = Join-Path $TsavRunPath 'TestEditorPerProjectUserSettings.ini'
    @'
[/Script/SuperTools.SuperDMXEditorSettings]
Protocol=ArtNet
Input=(bEnabled=False,LocalIp="127.0.0.1",Port=13654,StartUniverse=1)
Output=(bEnabled=False,LocalIp="127.0.0.1",RemoteIp="127.0.0.1",Port=13654,StartUniverse=1)
'@ | Set-Content -LiteralPath $TsavNetworkIni -Encoding utf8
    # Isolate both defaults and the generated user settings. A default-only
    # override would still merge the operator's saved network configuration.
    $TsavUserIni = Join-Path $TsavRunPath 'EditorPerProjectUserSettings.ini'
    Copy-Item -LiteralPath $TsavNetworkIni -Destination $TsavUserIni
    $TsavProject = Join-Path $TsavRoot 'LiveEventTest.uproject'
    $TsavEditor = Join-Path $UnrealRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $TsavSmokeArgs = @(
        ('"' + $TsavProject + '"'),
        '/Engine/Maps/Entry',
        '-EnablePlugins=SuperStage', '-Unattended', '-NoSound', '-NoSplash',
        ('-DEFEDITORPERPROJECTUSERSETTINGSINI="' + $TsavNetworkIni + '"'),
        ('-EDITORPERPROJECTUSERSETTINGSINI="' + $TsavUserIni + '"'),
        ('-ExecCmds="Automation RunTests ' + $TsavTestFilter + '"'),
        '-TestExit="Automation Test Queue Empty"',
        ('-ReportExportPath="' + $TsavRunPath + '"'),
        ('-abslog="' + (Join-Path $TsavRunPath 'editor.log') + '"')
    )
    $TsavSmokeArgs += $(if ($RenderSmoke) { '-RenderOffscreen' } else { '-NullRHI' })
    $TsavSmokeProcess = Start-Process -FilePath $TsavEditor -ArgumentList $TsavSmokeArgs -WindowStyle Hidden -PassThru
    if (-not $TsavSmokeProcess.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $TsavSmokeProcess.Id
        throw "SuperStage smoke test timed out. See $TsavRunPath"
    }
    $TsavReportFile = Join-Path $TsavRunPath 'index.json'
    if (-not (Test-Path -LiteralPath $TsavReportFile)) { throw "No automation report produced. See $TsavRunPath" }
    $TsavReport = Get-Content -LiteralPath $TsavReportFile -Raw | ConvertFrom-Json
    $TsavPassedTests = @($TsavReport.tests | Where-Object { $_.fullTestPath -in $TsavExpectedTests -and $_.state -eq 'Success' })
    if ($TsavSmokeProcess.ExitCode -ne 0 -or $TsavReport.failed -gt 0 -or $TsavPassedTests.Count -ne $TsavExpectedTests.Count) {
        throw "SuperStage smoke test failed. See $TsavRunPath"
    }
    Write-Host "SuperStage integration, shared patch, and lighting-console tests passed. Report: $TsavReportFile"
    if (-not $FixtureOutputProbe) {
        Write-Host 'Fixture output was not verified. After vendor activation, use -FixtureOutputProbe -RenderSmoke and confirm lighting in the viewport.'
    }
}

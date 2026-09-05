param(
    [Parameter(Mandatory = $true)]
    [string] $ArchivePath,
    [string] $UnrealRoot = 'C:\UE_5.8',
    [string] $SevenZipPath = 'C:\Program Files\7-Zip\7z.exe'
)
$ErrorActionPreference = 'Stop'
$TsavRoot = Split-Path -Parent $PSScriptRoot
$TsavDestination = Join-Path $TsavRoot 'Plugins\SuperStage'
if (Test-Path -LiteralPath $TsavDestination) {
    & (Join-Path $PSScriptRoot 'Test-SuperStage.ps1') -UnrealRoot $UnrealRoot
    Write-Host 'SuperStage is already installed. No files were replaced.'
    return
}
if (-not (Test-Path -LiteralPath $SevenZipPath)) { throw "7-Zip not found: $SevenZipPath" }
$TsavArchive = (Resolve-Path -LiteralPath $ArchivePath).Path
$TsavStaging = Join-Path $TsavRoot ('Saved\SuperStageInstall\' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $TsavStaging -Force | Out-Null

# Validate archive paths and links before allowing an extractor to write files.
$TsavPreviousEncoding = [Console]::OutputEncoding
try {
    [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
    $TsavListing = & $SevenZipPath l -slt -sccUTF-8 $TsavArchive
    $TsavListExit = $LASTEXITCODE
} finally {
    [Console]::OutputEncoding = $TsavPreviousEncoding
}
if ($TsavListExit -ne 0) { throw 'Unable to read SuperStage archive.' }
$TsavEntries = @()
foreach ($TsavBlock in (($TsavListing -join "`n") -split '\n\s*\n')) {
    if ($TsavBlock -notmatch '(?m)^Folder = ') { continue }
    $TsavEntryPath = [regex]::Match($TsavBlock, '(?m)^Path = (.+)$').Groups[1].Value.Trim()
    if (-not $TsavEntryPath -or $TsavEntryPath -match '(^|[\\/])\.\.([\\/]|$)|:|^[\\/]' -or
        $TsavBlock -match '(?m)^(Symbolic Link|Hard Link|Copy Link) = \S' -or
        $TsavBlock -match '(?m)^Alternate Stream = \+') {
        throw "Unsafe archive entry: $TsavEntryPath"
    }
    if ($TsavBlock -match '(?m)^Folder = -') {
        $TsavEntries += [pscustomobject]@{
            Path = $TsavEntryPath
            Size = [long][regex]::Match($TsavBlock, '(?m)^Size = (\d+)$').Groups[1].Value
        }
    }
}
if ($TsavEntries.Count -eq 0) { throw 'Archive has no files.' }
& $SevenZipPath x $TsavArchive "-o$TsavStaging" -y -bsp0
if ($LASTEXITCODE -ne 0) { throw 'SuperStage extraction/CRC check failed. Staging files retained for diagnosis.' }
foreach ($TsavEntry in $TsavEntries) {
    $TsavExtractedFile = Get-Item -LiteralPath (Join-Path $TsavStaging $TsavEntry.Path)
    if ($TsavExtractedFile.Length -ne $TsavEntry.Size) { throw "Extracted size mismatch: $($TsavEntry.Path)" }
}
$TsavDescriptors = @(Get-ChildItem -LiteralPath $TsavStaging -Filter '*.uplugin' -Recurse -File)
if ($TsavDescriptors.Count -ne 1 -or $TsavDescriptors[0].Name -ne 'SuperStage.uplugin') {
    throw 'Expected exactly one SuperStage.uplugin in the archive.'
}
$TsavSource = $TsavDescriptors[0].Directory.FullName
$TsavEngineManifest = Get-Content -LiteralPath (Join-Path $UnrealRoot 'Engine\Binaries\Win64\UnrealEditor.modules') -Raw | ConvertFrom-Json
$TsavVendorManifest = Get-Content -LiteralPath (Join-Path $TsavSource 'Binaries\Win64\UnrealEditor.modules') -Raw | ConvertFrom-Json
if ($TsavEngineManifest.BuildId -ne $TsavVendorManifest.BuildId) { throw 'Vendor build does not match this Unreal installation.' }

# Resolve and verify both paths before the recursive move. Never remove a prior install.
$TsavWorkspacePrefix = [IO.Path]::GetFullPath($TsavRoot) + [IO.Path]::DirectorySeparatorChar
$TsavDestination = [IO.Path]::GetFullPath($TsavDestination)
if (-not $TsavSource.StartsWith($TsavWorkspacePrefix, [StringComparison]::OrdinalIgnoreCase) -or
    -not $TsavDestination.StartsWith($TsavWorkspacePrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Installation paths must remain inside the project workspace.'
}
Move-Item -LiteralPath $TsavSource -Destination $TsavDestination
& (Join-Path $PSScriptRoot 'Test-SuperStage.ps1') -UnrealRoot $UnrealRoot
[pscustomobject]@{
    Archive = [IO.Path]::GetFileName($TsavArchive)
    SHA256 = (Get-FileHash -LiteralPath $TsavArchive -Algorithm SHA256).Hash
    Files = $TsavEntries.Count
    Bytes = ($TsavEntries | Measure-Object -Property Size -Sum).Sum
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $TsavStaging 'installation.json') -Encoding UTF8
Write-Host 'Installed the complete vendor distribution. Start with Start-TSAVSuperStage.cmd.'

param(
    [string]$Version = "",
    [string]$Platform = "x64",
    [string]$Configuration = "Release",
    [string]$ReleaseUrl = "",
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path $repoRoot "dist"
}

$pluginRoot = $repoRoot
$buildDir = Join-Path $repoRoot "build\$Platform\$Configuration\plugins\NppAIAssistant"
$dllPath = Join-Path $buildDir "NppAIAssistant.dll"
$metadataPath = Join-Path $pluginRoot "plugin-admin-metadata.json"

if (-not (Test-Path $dllPath)) {
    throw "Plugin DLL not found: $dllPath"
}
if (-not (Test-Path $metadataPath)) {
    throw "Plugin metadata file not found: $metadataPath"
}

$metadata = Get-Content $metadataPath -Raw | ConvertFrom-Json
$dllInfo = (Get-Item $dllPath).VersionInfo
$dllVersion = $dllInfo.ProductVersion
if ([string]::IsNullOrWhiteSpace($dllVersion)) {
    $dllVersion = $dllInfo.FileVersion
}
$dllVersion = ($dllVersion -replace '[^0-9\.]', '').Trim('.')

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $dllVersion
}

if ($Version -ne $dllVersion) {
    throw "Provided version '$Version' does not match DLL version '$dllVersion'."
}

$zipName = "$($metadata.folderName)-$Version-$Platform.zip"
$zipPath = Join-Path $OutDir $zipName
$stageRoot = Join-Path $OutDir "_stage\$($metadata.folderName)-$Version-$Platform"
$docRoot = Join-Path $stageRoot "doc\$($metadata.folderName)"

if (Test-Path $stageRoot) {
    Remove-Item $stageRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $docRoot | Out-Null

Copy-Item $dllPath (Join-Path $stageRoot "$($metadata.folderName).dll") -Force
Copy-Item (Join-Path $pluginRoot "README.md") (Join-Path $docRoot "README.md") -Force
if (Test-Path (Join-Path $repoRoot "README_zh-TW.md")) {
    Copy-Item (Join-Path $repoRoot "README_zh-TW.md") (Join-Path $docRoot "README_zh-TW.md") -Force
}
Copy-Item (Join-Path $repoRoot "LICENSE") (Join-Path $docRoot "LICENSE") -Force
Copy-Item (Join-Path $repoRoot "docs\USAGE.md") (Join-Path $docRoot "USAGE.md") -Force

if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Compress-Archive -Path (Join-Path $stageRoot "*") -DestinationPath $zipPath -CompressionLevel Optimal

$sha256 = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToUpperInvariant()

$pluginListEntry = [ordered]@{
    "folder-name" = $metadata.folderName
    "display-name" = $metadata.displayName
    "version" = $Version
    "id" = $sha256
    "repository" = $(if ([string]::IsNullOrWhiteSpace($ReleaseUrl)) { $metadata.repository } else { $ReleaseUrl })
    "description" = $metadata.description
    "author" = $metadata.author
    "homepage" = $metadata.homepage
}

$manifest = [ordered]@{
    folderName = $metadata.folderName
    displayName = $metadata.displayName
    version = $Version
    platform = $Platform
    zipPath = $zipPath
    sha256 = $sha256
    releaseUrl = $ReleaseUrl
    dllVersion = $dllVersion
    pluginListEntry = $pluginListEntry
}

$manifestPath = Join-Path $OutDir "$($metadata.folderName)-$Version-$Platform.plugin-admin.json"
$pluginListEntryPath = Join-Path $OutDir "$($metadata.folderName)-$Version-$Platform.npp-plugin-entry.json"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$manifest | ConvertTo-Json -Depth 6 | Set-Content $manifestPath -Encoding UTF8
$pluginListEntry | ConvertTo-Json -Depth 6 | Set-Content $pluginListEntryPath -Encoding UTF8

[pscustomobject]@{
    ZipPath = $zipPath
    Sha256 = $sha256
    ManifestPath = $manifestPath
    PluginListEntryPath = $pluginListEntryPath
    DllVersion = $dllVersion
    Repository = $pluginListEntry.repository
}

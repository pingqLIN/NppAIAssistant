param(
    [string]$Version = "",
    [ValidateSet("x64", "Win32", "ARM64")]
    [string]$Platform = "x64",
    [string]$Configuration = "Release",
    [string]$ReleaseUrl = "",
    [string]$OutDir = "",
    [string]$DllPath = "",
    [string]$ExpectedDllSha256 = "",
    [string]$SourceCommit = "",
    [switch]$Candidate
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path $repoRoot "dist"
}

$pluginRoot = $repoRoot
$buildDir = Join-Path $repoRoot "build\$Platform\$Configuration\plugins\NppAIAssistant"
if ([string]::IsNullOrWhiteSpace($DllPath)) {
    $DllPath = Join-Path $buildDir "NppAIAssistant.dll"
}
$pdbPath = Join-Path $buildDir "NppAIAssistant.pdb"
$metadataPath = Join-Path $pluginRoot "plugin-admin-metadata.json"

if (-not (Test-Path $DllPath)) {
    throw "Plugin DLL not found: $DllPath"
}
if (-not (Test-Path $metadataPath)) {
    throw "Plugin metadata file not found: $metadataPath"
}

if ($Candidate -and $SourceCommit -notmatch '^[0-9a-f]{40}$') {
    throw 'Candidate packages require a full source commit SHA.'
}
$dllSha256 = (Get-FileHash -LiteralPath $DllPath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($ExpectedDllSha256 -and $dllSha256 -ne $ExpectedDllSha256.ToUpperInvariant()) {
    throw 'DLL changed since validation; refusing to package.'
}

$metadata = Get-Content $metadataPath -Raw | ConvertFrom-Json
$dllInfo = (Get-Item $DllPath).VersionInfo
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

$packageStem = "$($metadata.folderName)-$Version-$Platform"
if ($Candidate) {
    $packageStem += "-candidate-$($SourceCommit.Substring(0, 8))"
}
$zipName = "$packageStem.zip"
$zipPath = Join-Path $OutDir $zipName
$stageRoot = Join-Path $OutDir "_stage\$packageStem"
$docRoot = Join-Path $stageRoot "doc\$($metadata.folderName)"

if (Test-Path $stageRoot) {
    Remove-Item $stageRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $docRoot | Out-Null

Copy-Item $DllPath (Join-Path $stageRoot "$($metadata.folderName).dll") -Force
$stagedHash = (Get-FileHash -LiteralPath (Join-Path $stageRoot "$($metadata.folderName).dll") -Algorithm SHA256).Hash
if ($stagedHash -ne $dllSha256) {
    throw 'Staged DLL differs from validated input.'
}
Copy-Item (Join-Path $pluginRoot "README.md") (Join-Path $docRoot "README.md") -Force
if (Test-Path (Join-Path $repoRoot "README_zh-TW.md")) {
    Copy-Item (Join-Path $repoRoot "README_zh-TW.md") (Join-Path $docRoot "README_zh-TW.md") -Force
}
Copy-Item (Join-Path $repoRoot "LICENSE") (Join-Path $docRoot "LICENSE") -Force
Copy-Item (Join-Path $repoRoot "docs\USAGE.md") (Join-Path $docRoot "USAGE.md") -Force
if (Test-Path (Join-Path $repoRoot "docs\DEVELOPMENT_LOG.md")) {
    Copy-Item (Join-Path $repoRoot "docs\DEVELOPMENT_LOG.md") (Join-Path $docRoot "DEVELOPMENT_LOG.md") -Force
}
if (Test-Path (Join-Path $repoRoot "docs\SECURITY_REMEDIATION.md")) {
    Copy-Item (Join-Path $repoRoot "docs\SECURITY_REMEDIATION.md") (Join-Path $docRoot "SECURITY_REMEDIATION.md") -Force
}
if (Test-Path (Join-Path $repoRoot "docs\SECURITY_VERIFICATION.md")) {
    Copy-Item (Join-Path $repoRoot "docs\SECURITY_VERIFICATION.md") (Join-Path $docRoot "SECURITY_VERIFICATION.md") -Force
}

if (Test-Path $pdbPath) {
    Write-Host "Local PDB detected at $pdbPath. It will not be included in the release package."
}

$stagedSymbols = Get-ChildItem -Path $stageRoot -Filter *.pdb -Recurse -ErrorAction SilentlyContinue
if ($stagedSymbols) {
    $symbolList = ($stagedSymbols | ForEach-Object { $_.FullName }) -join [Environment]::NewLine
    throw "Refusing to package symbol files:`n$symbolList"
}

if (Test-Path $zipPath) {
    throw "Package already exists; use a new output directory: $zipPath"
}

Compress-Archive -Path (Join-Path $stageRoot "*") -DestinationPath $zipPath -CompressionLevel Optimal

$sha256 = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToUpperInvariant()
"$sha256  $zipName" | Set-Content -LiteralPath "$zipPath.sha256" -Encoding ascii

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
    dllSha256 = $dllSha256
    sourceCommit = $SourceCommit
    candidate = [bool]$Candidate
    pluginListEntry = $(if ($Candidate) { $null } else { $pluginListEntry })
}

$manifestPath = Join-Path $OutDir "$packageStem.plugin-admin.json"
$pluginListEntryPath = Join-Path $OutDir "$packageStem.npp-plugin-entry.json"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$manifest | ConvertTo-Json -Depth 6 | Set-Content $manifestPath -Encoding UTF8
if (-not $Candidate) {
    $pluginListEntry | ConvertTo-Json -Depth 6 | Set-Content $pluginListEntryPath -Encoding UTF8
} else {
    $pluginListEntryPath = $null
}

[pscustomobject]@{
    ZipPath = $zipPath
    Sha256 = $sha256
    ManifestPath = $manifestPath
    PluginListEntryPath = $pluginListEntryPath
    DllVersion = $dllVersion
    Repository = $pluginListEntry.repository
}

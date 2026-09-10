[CmdletBinding()]
param(
    [string]$Platform = "x64",
    [string]$Configuration = "Release",
    [string]$Version = "",
    [switch]$RequirePluginsAdminReady,
    [string]$ManifestPath = "",
    [string]$DllPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $manifestPath = Join-Path $repoRoot "dist\NppAIAssistant-$Version-$Platform.plugin-admin.json"
    if ([string]::IsNullOrWhiteSpace($Version)) {
        $manifestCandidates = Get-ChildItem -Path (Join-Path $repoRoot "dist") -Filter "NppAIAssistant-*-$Platform.plugin-admin.json" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTimeUtc -Descending
        if (-not $manifestCandidates) {
            throw "No package manifest found for platform '$Platform'. Run scripts/package-npp-ai-plugin.ps1 first."
        }
        $manifestPath = $manifestCandidates[0].FullName
    }
}

if (-not (Test-Path $manifestPath)) {
    throw "Package manifest not found: $manifestPath"
}

$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$zipPath = $manifest.zipPath
if ([string]::IsNullOrWhiteSpace($DllPath)) {
    $dllPath = Join-Path $repoRoot "build\$Platform\$Configuration\plugins\NppAIAssistant\NppAIAssistant.dll"
}
$mainSourcePath = Join-Path $repoRoot "src\NppAIAssistant.cpp"

function Assert-Check {
    param(
        [string]$Name,
        [bool]$Passed,
        [string]$Failure
    )

    if (-not $Passed) {
        throw "$Name failed. $Failure"
    }

    Write-Host "PASS: $Name"
}

function Test-DirectHttpsZipUrl {
    param([string]$Url)

    $parsedUrl = $null
    if (-not [System.Uri]::TryCreate($Url, [System.UriKind]::Absolute, [ref]$parsedUrl)) {
        return $false
    }

    return $parsedUrl.Scheme -eq "https" -and
        $parsedUrl.AbsolutePath.EndsWith(".zip", [System.StringComparison]::OrdinalIgnoreCase)
}

Add-Type -AssemblyName System.IO.Compression.FileSystem

Assert-Check "Manifest platform matches request" ($manifest.platform -eq $Platform) "Manifest platform '$($manifest.platform)' does not match '$Platform'."
Assert-Check "Manifest zip exists" (Test-Path $zipPath) "Zip path does not exist: $zipPath"
Assert-Check "Build DLL exists" (Test-Path $dllPath) "Build DLL does not exist: $dllPath"

$dllInfo = (Get-Item $dllPath).VersionInfo
$dllVersion = $dllInfo.ProductVersion
if ([string]::IsNullOrWhiteSpace($dllVersion)) {
    $dllVersion = $dllInfo.FileVersion
}
$dllVersion = ($dllVersion -replace '[^0-9\.]', '').Trim('.')

Assert-Check "DLL version matches manifest" ($dllVersion -eq $manifest.version) "DLL version '$dllVersion' does not match manifest version '$($manifest.version)'."
Assert-Check "Manifest DLL version is recorded" ($manifest.dllVersion -eq $manifest.version) "Manifest dllVersion '$($manifest.dllVersion)' does not match version '$($manifest.version)'."

$computedHash = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToUpperInvariant()
Assert-Check "Package hash matches manifest" ($computedHash -eq $manifest.sha256) "Manifest SHA-256 '$($manifest.sha256)' does not match computed '$computedHash'."

$zip = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    $dllEntry = $zip.GetEntry("NppAIAssistant.dll")
    Assert-Check "Package DLL exists" ($null -ne $dllEntry) "Root DLL is missing."
    $entryStream = $dllEntry.Open()
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $packedDllHash = [System.BitConverter]::ToString($sha.ComputeHash($entryStream)).Replace("-", "")
    } finally { $sha.Dispose(); $entryStream.Dispose() }
    $buildDllHash = (Get-FileHash -LiteralPath $DllPath -Algorithm SHA256).Hash
    Assert-Check "Package DLL matches tested build" ($packedDllHash -eq $buildDllHash) "ZIP contains a different DLL."
    $entryNames = @($zip.Entries | ForEach-Object { $_.FullName -replace '\\', '/' })
    Assert-Check "Package has root DLL" ($entryNames -contains "NppAIAssistant.dll") "NppAIAssistant.dll must be at zip root."
    Assert-Check "Package has no PDB files" (-not ($entryNames | Where-Object { $_.EndsWith(".pdb", [System.StringComparison]::OrdinalIgnoreCase) })) "PDB files must not be included in release zips."
    foreach ($docName in @("README.md", "USAGE.md", "LICENSE")) {
        Assert-Check "Package doc $docName is present" ($entryNames -contains "doc/NppAIAssistant/$docName") "Missing doc/NppAIAssistant/$docName."
    }
}
finally {
    $zip.Dispose()
}

$pluginListEntry = $manifest.pluginListEntry
Assert-Check "Plugin list folder-name matches DLL basename" ($pluginListEntry."folder-name" -eq "NppAIAssistant") "folder-name must be NppAIAssistant."
Assert-Check "Plugin list version matches manifest" ($pluginListEntry.version -eq $manifest.version) "Entry version '$($pluginListEntry.version)' does not match manifest version '$($manifest.version)'."
Assert-Check "Plugin list id matches package hash" ($pluginListEntry.id -eq $manifest.sha256) "Entry id must be the package SHA-256."

if ($manifest.pluginsAdminReady) {
    Assert-Check "Plugin list repository is a direct HTTPS zip" (Test-DirectHttpsZipUrl $pluginListEntry.repository) "Repository must be a direct HTTPS .zip URL when PluginsAdminReady is true."
    Assert-Check "Manifest releaseUrl matches repository" ($manifest.releaseUrl -eq $pluginListEntry.repository) "releaseUrl must match pluginListEntry.repository."
}
elseif ($RequirePluginsAdminReady) {
    throw "Plugins Admin readiness is false: $($manifest.pluginsAdminReadiness)"
}
else {
    Write-Host "INFO: Plugins Admin readiness is false: $($manifest.pluginsAdminReadiness)"
}

$mainSource = Get-Content $mainSourcePath -Raw
foreach ($exportName in @("setInfo", "getName", "getFuncsArray", "beNotified", "messageProc", "isUnicode")) {
    $escapedName = [regex]::Escape($exportName)
    Assert-Check "Source exports $exportName" ($mainSource -match "__declspec\(dllexport\).*$escapedName") "Missing Notepad++ plugin export '$exportName'."
}

[pscustomobject]@{
    Platform = $Platform
    Version = $manifest.version
    ZipPath = $zipPath
    Sha256 = $computedHash
    PluginsAdminReady = [bool]$manifest.pluginsAdminReady
    Repository = $pluginListEntry.repository
}

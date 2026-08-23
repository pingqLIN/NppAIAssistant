[CmdletBinding()]
param(
    [string]$Platform = "x64",
    [string]$Configuration = "Release",
    [string]$Version = "",
    [switch]$RequirePluginsAdminReady
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($Version)) {
    $manifestCandidate = Get-ChildItem -Path (Join-Path $repoRoot "dist") -Filter "NppAIAssistant-*-$Platform.plugin-admin.json" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if (-not $manifestCandidate) {
        throw "No package manifest found for platform '$Platform'. Run scripts/package-npp-ai-plugin.ps1 first."
    }
    $manifestPath = $manifestCandidate.FullName
} else {
    $manifestPath = Join-Path $repoRoot "dist\NppAIAssistant-$Version-$Platform.plugin-admin.json"
}

if (-not (Test-Path $manifestPath)) {
    throw "Package manifest not found: $manifestPath"
}

$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$dllCandidates = @(
    (Join-Path $repoRoot "build\$Platform\$Configuration\plugins\NppAIAssistant\NppAIAssistant.dll"),
    (Join-Path $repoRoot "build\$Platform\$Configuration\plugins\NppAIAssistant\$Configuration\NppAIAssistant.dll")
)
$dllPath = $dllCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

function Assert-Check {
    param([string]$Name, [bool]$Passed, [string]$Failure)
    if (-not $Passed) { throw "$Name failed. $Failure" }
    Write-Host "PASS: $Name"
}

function Test-DirectHttpsZipUrl {
    param([string]$Url)
    $parsedUrl = $null
    return [System.Uri]::TryCreate($Url, [System.UriKind]::Absolute, [ref]$parsedUrl) -and
        $parsedUrl.Scheme -eq 'https' -and
        $parsedUrl.AbsolutePath.EndsWith('.zip', [System.StringComparison]::OrdinalIgnoreCase)
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
Assert-Check "Manifest platform matches request" ($manifest.platform -eq $Platform) "Manifest platform '$($manifest.platform)' does not match '$Platform'."
Assert-Check "Manifest zip exists" (Test-Path $manifest.zipPath) "Zip path does not exist: $($manifest.zipPath)"
Assert-Check "Build DLL exists" (-not [string]::IsNullOrWhiteSpace($dllPath)) "Checked: $($dllCandidates -join '; ')"

$dllInfo = (Get-Item $dllPath).VersionInfo
$dllVersion = ($dllInfo.ProductVersion ?? $dllInfo.FileVersion) -replace '[^0-9\.]', ''
$dllVersion = $dllVersion.Trim('.')
Assert-Check "DLL version matches manifest" ($dllVersion -eq $manifest.version) "DLL version '$dllVersion' does not match '$($manifest.version)'."
Assert-Check "Package hash matches manifest" ((Get-FileHash $manifest.zipPath -Algorithm SHA256).Hash.ToUpperInvariant() -eq $manifest.sha256) "Package hash differs from manifest."

$zip = [System.IO.Compression.ZipFile]::OpenRead($manifest.zipPath)
try {
    $entryNames = @($zip.Entries | ForEach-Object { $_.FullName -replace '\\', '/' })
    Assert-Check "Package has root DLL" ($entryNames -contains 'NppAIAssistant.dll') 'NppAIAssistant.dll must be at the ZIP root.'
    Assert-Check "Package has no PDB files" (-not ($entryNames | Where-Object { $_.EndsWith('.pdb', [System.StringComparison]::OrdinalIgnoreCase) })) 'PDB files must not be included.'
    foreach ($docName in @('README.md', 'README_zh-TW.md', 'USAGE.md', 'LOCAL_PROVIDER_AND_TIMEOUT.md', 'CHANGELOG.md', 'LICENSE')) {
        Assert-Check "Package doc $docName is present" ($entryNames -contains "doc/NppAIAssistant/$docName") "Missing doc/NppAIAssistant/$docName."
    }
}
finally { $zip.Dispose() }

$entry = $manifest.pluginListEntry
Assert-Check "Plugin list folder-name matches DLL basename" ($entry.'folder-name' -eq 'NppAIAssistant') 'folder-name must match the DLL basename.'
Assert-Check "Plugin list version matches manifest" ($entry.version -eq $manifest.version) 'Plugin-list version differs from manifest.'
Assert-Check "Plugin list id matches package hash" ($entry.id -eq $manifest.sha256) 'Plugin-list id must be the ZIP SHA-256.'

if ($manifest.pluginsAdminReady) {
    Assert-Check "Plugin list repository is a direct HTTPS zip" (Test-DirectHttpsZipUrl $entry.repository) 'Repository must be a direct HTTPS .zip URL.'
} elseif ($RequirePluginsAdminReady) {
    throw "Plugins Admin readiness is false: $($manifest.pluginsAdminReadiness)"
} else {
    Write-Host "INFO: Plugins Admin readiness is false: $($manifest.pluginsAdminReadiness)"
}

$mainSource = Get-Content (Join-Path $repoRoot 'src\NppAIAssistant.cpp') -Raw
foreach ($exportName in @('setInfo', 'getName', 'getFuncsArray', 'beNotified', 'messageProc', 'isUnicode')) {
    Assert-Check "Source exports $exportName" ($mainSource -match "__declspec\(dllexport\).*$([regex]::Escape($exportName))") "Missing Notepad++ plugin export '$exportName'."
}

[pscustomobject]@{
    Platform = $Platform
    Version = $manifest.version
    ZipPath = $manifest.zipPath
    Sha256 = $manifest.sha256
    PluginsAdminReady = [bool]$manifest.pluginsAdminReady
}

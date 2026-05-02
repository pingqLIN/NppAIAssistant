param(
    [string]$ZipPath = "",
    [string]$OutDir = "",
    [switch]$RequirePluginsAdminReady
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($ZipPath)) {
    $manifestCandidates = Get-ChildItem -Path (Join-Path $repoRoot "dist") -Filter "*.plugin-admin.json" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending
    if (-not $manifestCandidates) {
        throw "No package manifest found under dist. Run scripts/package-npp-ai-plugin.ps1 first."
    }

    $manifest = Get-Content $manifestCandidates[0].FullName -Raw | ConvertFrom-Json
    $ZipPath = $manifest.zipPath
} else {
    $manifestPath = [System.IO.Path]::ChangeExtension($ZipPath, ".plugin-admin.json")
    $manifest = $null
}

$zipItem = Get-Item $ZipPath
if (-not $manifest) {
    $manifestName = "$($zipItem.BaseName).plugin-admin.json"
    $manifestFile = Join-Path $zipItem.DirectoryName $manifestName
    if (Test-Path $manifestFile) {
        $manifest = Get-Content $manifestFile -Raw | ConvertFrom-Json
    }
}

if (-not $manifest) {
    throw "Package manifest not found for $ZipPath."
}

if ($RequirePluginsAdminReady -and -not $manifest.pluginsAdminReady) {
    throw "Package is not Plugins Admin ready: $($manifest.pluginsAdminReadiness)"
}

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $OutDir = Join-Path $repoRoot "dist\smoke-install"
}

$smokeRoot = Join-Path $OutDir "$($manifest.folderName)-$($manifest.version)-$($manifest.platform)"
$extractRoot = Join-Path $smokeRoot "zip-root"
$pluginInstallRoot = Join-Path $smokeRoot "Notepad++\plugins\$($manifest.folderName)"

if (Test-Path $smokeRoot) {
    Remove-Item $smokeRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null
Expand-Archive -Path $zipItem.FullName -DestinationPath $extractRoot -Force

$rootDllPath = Join-Path $extractRoot "$($manifest.folderName).dll"
if (-not (Test-Path $rootDllPath)) {
    throw "Package root DLL is missing: $rootDllPath"
}

$pdbFiles = Get-ChildItem -Path $extractRoot -Filter "*.pdb" -Recurse -ErrorAction SilentlyContinue
if ($pdbFiles) {
    $pdbList = ($pdbFiles | ForEach-Object { $_.FullName }) -join [Environment]::NewLine
    throw "Package contains PDB files:`n$pdbList"
}

$docRoot = Join-Path $extractRoot "doc\$($manifest.folderName)"
$requiredDocs = @("README.md", "USAGE.md", "LICENSE")
foreach ($requiredDoc in $requiredDocs) {
    $docPath = Join-Path $docRoot $requiredDoc
    if (-not (Test-Path $docPath)) {
        throw "Package doc is missing: $docPath"
    }
}

New-Item -ItemType Directory -Force -Path $pluginInstallRoot | Out-Null
Copy-Item $rootDllPath (Join-Path $pluginInstallRoot "$($manifest.folderName).dll") -Force

$computedHash = (Get-FileHash $zipItem.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
if ($computedHash -ne $manifest.sha256) {
    throw "Package SHA-256 mismatch. Manifest=$($manifest.sha256) Computed=$computedHash"
}

[pscustomobject]@{
    ZipPath = $zipItem.FullName
    Sha256 = $computedHash
    PluginsAdminReady = [bool]$manifest.pluginsAdminReady
    PluginsAdminReadiness = $manifest.pluginsAdminReadiness
    ExtractRoot = $extractRoot
    PluginInstallRoot = $pluginInstallRoot
    RootDllPresent = $true
    PdbPresent = $false
    RequiredDocsPresent = $true
}

[CmdletBinding()]
param(
    [string]$RepoRoot = "",
    [string]$PluginListRepo = "",
    [ValidateSet("x86", "x64", "arm64")]
    [string]$Platform = "x64",
    [ValidateSet("Development", "Release")]
    [string]$Mode = "Development",
    [switch]$AsJson
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Join-Path $PSScriptRoot ".."
}
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path

if ([string]::IsNullOrWhiteSpace($PluginListRepo)) {
    $PluginListRepo = Join-Path (Split-Path -Parent $RepoRoot) "nppPluginList"
}

$results = New-Object 'System.Collections.Generic.List[object]'
function Add-Check {
    param(
        [string]$Name,
        [ValidateSet("PASS", "WARNING", "FAIL")]
        [string]$State,
        [string]$Detail
    )

    [void]$results.Add([pscustomobject]@{
        Name = $Name
        State = $State
        Detail = $Detail
    })
}

foreach ($directory in @("src", "vendor", "docs", "scripts")) {
    $path = Join-Path $RepoRoot $directory
    Add-Check "Directory:$directory" $(if (Test-Path -LiteralPath $path -PathType Container) { "PASS" } else { "FAIL" }) $path
}

foreach ($file in @("CMakeLists.txt", "NppAIAssistant.vcxproj", "plugin-admin-metadata.json", "scripts\\package-npp-ai-plugin.ps1", "scripts\\smoke-package-install.ps1", "scripts\\verify-release-readiness.ps1", "scripts\\Get-NppPluginOfficialState.ps1")) {
    $path = Join-Path $RepoRoot $file
    Add-Check "File:$file" $(if (Test-Path -LiteralPath $path -PathType Leaf) { "PASS" } else { "FAIL" }) $path
}

$metadataPath = Join-Path $RepoRoot "plugin-admin-metadata.json"
try {
    $metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
    $folderName = [string]$metadata.folderName
    Add-Check "Metadata:folderName" $(if ([string]::IsNullOrWhiteSpace($folderName)) { "FAIL" } else { "PASS" }) $folderName
}
catch {
    $folderName = ""
    Add-Check "Metadata:parse" "FAIL" $_.Exception.Message
}

$gitIgnorePath = Join-Path $RepoRoot ".gitignore"
try {
    $gitIgnore = Get-Content -LiteralPath $gitIgnorePath -Raw
    foreach ($rule in @("build/", "build-*/", "dist/", ".clean/")) {
        Add-Check "GitIgnore:$rule" $(if ($gitIgnore -match [regex]::Escape($rule)) { "PASS" } else { "FAIL" }) $rule
    }
}
catch {
    Add-Check "GitIgnore:parse" "FAIL" $_.Exception.Message
}

$workspacePath = Join-Path $RepoRoot "NppAIAssistant.code-workspace"
try {
    $workspace = Get-Content -LiteralPath $workspacePath -Raw | ConvertFrom-Json
    $workspacePaths = @($workspace.folders | ForEach-Object { [string]$_.path })
    $sourceOnly = ($workspacePaths.Count -eq 1 -and $workspacePaths[0] -eq ".")
    Add-Check "Workspace:source-only" $(if ($sourceOnly) { "PASS" } else { "FAIL" }) ($workspacePaths -join ", ")
}
catch {
    Add-Check "Workspace:parse" "FAIL" $_.Exception.Message
}

try {
    $worktreeLines = @(git -C $RepoRoot worktree list --porcelain)
    $normalizedRoot = $RepoRoot.Replace("\\", "/")
    $worktreePaths = @($worktreeLines | Where-Object { $_.StartsWith("worktree ") } | ForEach-Object { $_.Substring(9).Replace("\\", "/") })
    $nested = @($worktreePaths | Where-Object { $_ -ne $normalizedRoot -and $_.StartsWith("$normalizedRoot/") })
    Add-Check "Worktrees:outside-primary" $(if ($nested.Count -eq 0) { "PASS" } else { "FAIL" }) $(if ($nested.Count -eq 0) { "No linked worktree is nested under the primary checkout." } else { $nested -join "; " })
}
catch {
    Add-Check "Worktrees:registration" "FAIL" $_.Exception.Message
}

$distPath = Join-Path $RepoRoot "dist"
$zipItems = @(Get-ChildItem -LiteralPath $distPath -Filter "*-$Platform.zip" -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTimeUtc -Descending)
if ($zipItems.Count -eq 0) {
    Add-Check "Package:local-zip" $(if ($Mode -eq "Release") { "FAIL" } else { "WARNING" }) "No local package ZIP exists."
}
elseif ([string]::IsNullOrWhiteSpace($folderName)) {
    Add-Check "Package:layout" "FAIL" "Cannot inspect package without metadata.folderName."
}
else {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zipItem = $zipItems[0]
    $archive = [System.IO.Compression.ZipFile]::OpenRead($zipItem.FullName)
    try {
        $entries = @($archive.Entries | ForEach-Object { $_.FullName })
        $rootDll = "$folderName.dll"
        Add-Check "Package:root-dll" $(if ($entries -contains $rootDll) { "PASS" } else { "FAIL" }) "$($zipItem.Name): $rootDll"
        $docsPresent = @($entries | Where-Object { $_ -like "doc/$folderName/*" }).Count -gt 0
        Add-Check "Package:docs" $(if ($docsPresent) { "PASS" } else { "FAIL" }) "$($zipItem.Name): doc/$folderName/"
        $pdbPresent = @($entries | Where-Object { $_ -like "*.pdb" }).Count -gt 0
        Add-Check "Package:no-pdb" $(if (-not $pdbPresent) { "PASS" } else { "FAIL" }) $zipItem.Name
    }
    finally {
        $archive.Dispose()
    }

    $manifestPath = Join-Path $distPath "$($zipItem.BaseName).plugin-admin.json"
    if (Test-Path -LiteralPath $manifestPath) {
        try {
            $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
            $zipHash = (Get-FileHash -LiteralPath $zipItem.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
            Add-Check "Package:manifest-hash" $(if ($manifest.sha256 -eq $zipHash) { "PASS" } else { "FAIL" }) $zipItem.Name
        }
        catch {
            Add-Check "Package:manifest-hash" "FAIL" $_.Exception.Message
        }
    }
    else {
        Add-Check "Package:manifest" $(if ($Mode -eq "Release") { "FAIL" } else { "WARNING" }) $manifestPath
    }

    $pluginListPath = Join-Path $PluginListRepo "src\\pl.$Platform.json"
    if (Test-Path -LiteralPath $pluginListPath) {
        try {
            $pluginList = Get-Content -LiteralPath $pluginListPath -Raw | ConvertFrom-Json
            $entry = @($pluginList.'npp-plugins' | Where-Object { $_.'folder-name' -eq $folderName }) | Select-Object -First 1
            if ($null -eq $entry) {
                Add-Check "PluginList:entry" $(if ($Mode -eq "Release") { "FAIL" } else { "WARNING" }) "No local $Platform entry for $folderName."
            }
            else {
                $zipHash = (Get-FileHash -LiteralPath $zipItem.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
                $state = if ($entry.id -eq $zipHash) { "PASS" } elseif ($Mode -eq "Release") { "FAIL" } else { "WARNING" }
                Add-Check "PluginList:package-hash" $state "ZIP=$zipHash; list=$($entry.id)"
            }
        }
        catch {
            Add-Check "PluginList:entry" "FAIL" $_.Exception.Message
        }
    }
    else {
        Add-Check "PluginList:checkout" "WARNING" "No local nppPluginList checkout at $PluginListRepo."
    }
}

$failed = @($results | Where-Object { $_.State -eq "FAIL" }).Count -gt 0
$output = [pscustomobject]@{
    RepoRoot = $RepoRoot
    PluginListRepo = $PluginListRepo
    Platform = $Platform
    Mode = $Mode
    Passed = -not $failed
    Checks = $results
}

if ($AsJson) {
    $output | ConvertTo-Json -Depth 5
}
else {
    $results | Format-Table -AutoSize
    Write-Output "Overall: $(if ($failed) { 'FAIL' } else { 'PASS' })"
}

if ($failed) {
    exit 1
}

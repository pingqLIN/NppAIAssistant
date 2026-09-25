[CmdletBinding()]
param(
    [ValidateSet("x86", "x64", "arm64")]
    [string]$Platform = "x64",
    [string]$RepoRoot = "",
    [string]$PluginRepository = "",
    [ValidateRange(5, 120)]
    [int]$TimeoutSec = 30,
    [switch]$RequireOnline
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Join-Path $PSScriptRoot ".."
}
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$metadata = Get-Content -LiteralPath (Join-Path $RepoRoot "plugin-admin-metadata.json") -Raw | ConvertFrom-Json
$folderName = [string]$metadata.folderName

if ([string]::IsNullOrWhiteSpace($PluginRepository)) {
    $homepage = [uri][string]$metadata.homepage
    if ($homepage.Host -ne "github.com" -or $homepage.Segments.Count -lt 3) {
        throw "PluginRepository was not supplied and metadata.homepage is not a GitHub repository URL."
    }
    $PluginRepository = "$($homepage.Segments[1].Trim('/'))/$($homepage.Segments[2].Trim('/'))"
}

$headers = @{ "Accept" = "application/vnd.github+json"; "User-Agent" = "NppAIAssistant-official-state-check" }
$errors = New-Object 'System.Collections.Generic.List[string]'
$listUrl = "https://raw.githubusercontent.com/notepad-plus-plus/nppPluginList/master/src/pl.$Platform.json"
$releaseUrl = "https://api.github.com/repos/$PluginRepository/releases/latest"
$listState = [ordered]@{ Source = $listUrl; Status = "UNKNOWN"; Entry = $null; Error = $null }
$releaseState = [ordered]@{ Source = $releaseUrl; Status = "UNKNOWN"; Tag = $null; PublishedAt = $null; ZipAssets = @(); Error = $null }

try {
    $pluginList = Invoke-RestMethod -Uri $listUrl -Headers $headers -TimeoutSec $TimeoutSec
    $entry = @($pluginList.'npp-plugins' | Where-Object { $_.'folder-name' -eq $folderName }) | Select-Object -First 1
    $listState.Status = "VERIFIED"
    if ($null -eq $entry) {
        $listState.Entry = [pscustomobject]@{ Found = $false }
    }
    else {
        $listState.Entry = [pscustomobject]@{
            Found = $true
            FolderName = $entry.'folder-name'
            Version = $entry.version
            Id = $entry.id
            Repository = $entry.repository
        }
    }
}
catch {
    $listState.Error = $_.Exception.Message
    [void]$errors.Add("Plugin List: $($_.Exception.Message)")
}

try {
    $release = Invoke-RestMethod -Uri $releaseUrl -Headers $headers -TimeoutSec $TimeoutSec
    $releaseState.Status = "VERIFIED"
    $releaseState.Tag = $release.tag_name
    $releaseState.PublishedAt = $release.published_at
    $releaseState.ZipAssets = @($release.assets | Where-Object { $_.browser_download_url -match '\.zip$' } | ForEach-Object {
        [pscustomobject]@{ Name = $_.name; Url = $_.browser_download_url; Size = $_.size }
    })
}
catch {
    $releaseState.Error = $_.Exception.Message
    [void]$errors.Add("Plugin release: $($_.Exception.Message)")
}

$alignment = "UNKNOWN"
if ($listState.Status -eq "VERIFIED" -and $releaseState.Status -eq "VERIFIED" -and $null -ne $listState.Entry -and $listState.Entry.Found) {
    $matchingAsset = @($releaseState.ZipAssets | Where-Object { $_.Url -eq $listState.Entry.Repository }) | Select-Object -First 1
    $alignment = if ($null -ne $matchingAsset) { "REPOSITORY_URL_MATCH" } else { "REPOSITORY_URL_MISMATCH_OR_OLDER_RELEASE" }
}

[pscustomobject]@{
    Plugin = $folderName
    Platform = $Platform
    PluginRepository = $PluginRepository
    Online = ($errors.Count -eq 0)
    PluginList = [pscustomobject]$listState
    GitHubRelease = [pscustomobject]$releaseState
    Alignment = $alignment
    HashComparison = "UNKNOWN: this read-only refresh does not download release ZIP assets."
    Errors = $errors
} | ConvertTo-Json -Depth 7

if ($RequireOnline -and $errors.Count -gt 0) {
    exit 1
}

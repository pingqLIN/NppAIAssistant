[CmdletBinding()]
param(
  [string]$Configuration = 'Release',
  [string]$Platform = 'x64',
  [string]$ProjectFile = 'NppAIAssistant.vcxproj',
  [string]$RepoRoot = '',
  [string]$LogPath = '',
  [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $RepoRoot) {
  $RepoRoot = (Get-Location).Path
}
$repoRoot = (Resolve-Path $RepoRoot).Path
if (-not [System.IO.Path]::IsPathRooted($ProjectFile)) {
  $ProjectFile = Join-Path $repoRoot $ProjectFile
}
if (-not $LogPath) {
  $LogPath = Join-Path $repoRoot '.codex\project-development-loop\msbuild-filetracker-doctor.log'
}

function Ensure-ParentDirectory {
  param([string]$Path)
  $directory = Split-Path -Parent $Path
  if (-not (Test-Path $directory)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
  }
}

Ensure-ParentDirectory -Path $LogPath

$interestingEnvVars = @(
  'APPDATA',
  'ProgramData',
  'LOCALAPPDATA',
  'SystemDrive',
  'TEMP',
  'TMP',
  'USERPROFILE',
  'SystemRoot',
  'VCTargetsPath',
  'UserRootDir'
)

$report = New-Object System.Collections.Generic.List[string]
$report.Add("timestampUtc=$((Get-Date).ToUniversalTime().ToString('o'))")
$report.Add("repoRoot=$repoRoot")
$report.Add("projectFile=$ProjectFile")
$report.Add("configuration=$Configuration")
$report.Add("platform=$Platform")
$report.Add('environment:')
foreach ($name in $interestingEnvVars) {
  $value = [Environment]::GetEnvironmentVariable($name)
  $report.Add("  $name=$value")
}
$report.Add("projectExists=$(Test-Path $ProjectFile)")
$report.Add("cwd=$((Get-Location).Path)")

if (-not $SkipBuild) {
  $wrapper = Join-Path $repoRoot 'scripts\invoke-msbuild.ps1'
  $report.Add("msbuildWrapper=$wrapper")
  $report.Add('msbuildAttempt=begin-with-sanitized-environment')
  try {
    $buildOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $wrapper -RepoRoot $repoRoot -ProjectFile $ProjectFile -Configuration $Configuration -Platform $Platform 2>&1
    foreach ($line in @($buildOutput)) {
      $report.Add([string]$line)
    }
    $report.Add("msbuildExitCode=$LASTEXITCODE")
  } catch {
    $report.Add("msbuildException=$($_.Exception.Message)")
  }
} else {
  $report.Add('msbuildAttempt=skipped')
}

$report | Set-Content -LiteralPath $LogPath -Encoding UTF8
Get-Content $LogPath

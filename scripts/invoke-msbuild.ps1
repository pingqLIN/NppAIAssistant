[CmdletBinding()]
param(
  [string]$Configuration = 'Release',
  [string]$Platform = 'x64',
  [string]$ProjectFile = 'NppAIAssistant.vcxproj',
  [string]$RepoRoot = '',
  [string[]]$ExtraArguments = @()
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

function Set-DefaultEnvironmentValue {
  param(
    [string]$Name,
    [string]$Value
  )

  if (-not [Environment]::GetEnvironmentVariable($Name)) {
    Set-Item -Path "Env:$Name" -Value $Value
  }
}

Set-DefaultEnvironmentValue -Name 'SystemDrive' -Value 'C:'
Set-DefaultEnvironmentValue -Name 'LOCALAPPDATA' -Value (Join-Path $env:USERPROFILE 'AppData\Local')
Set-DefaultEnvironmentValue -Name 'ProgramData' -Value 'C:\ProgramData'

$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
$arguments = @(
  $ProjectFile,
  "/p:Configuration=$Configuration",
  "/p:Platform=$Platform",
  '/m'
) + $ExtraArguments

& $msbuild @arguments
exit $LASTEXITCODE

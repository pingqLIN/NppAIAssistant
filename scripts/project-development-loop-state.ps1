[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet('start', 'checkpoint', 'finish', 'show')]
  [string]$Action,

  [string]$Batch = '',
  [string]$Checkpoint = '',
  [string]$NextAction = '',
  [string]$Notes = '',
  [string]$Mode = '',
  [string]$Blockers = '',
  [string]$TelemetryStatus = '',
  [int]$DurationHours = 8,
  [string]$StatePath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

if (-not $StatePath) {
  $StatePath = Join-Path $repoRoot '.codex\project-development-loop\active-run.json'
} elseif (-not [System.IO.Path]::IsPathRooted($StatePath)) {
  $StatePath = Join-Path $repoRoot $StatePath
}

function Ensure-StateDirectory {
  param([string]$Path)
  $directory = Split-Path -Parent $Path
  if (-not (Test-Path $directory)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
  }
}

function Read-State {
  param([string]$Path)
  if (-not (Test-Path $Path)) {
    return $null
  }

  return Get-Content $Path -Raw | ConvertFrom-Json
}

function Write-State {
  param(
    [string]$Path,
    [pscustomobject]$State
  )

  Ensure-StateDirectory -Path $Path
  $State | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function New-Timestamp {
  return (Get-Date).ToUniversalTime().ToString('o')
}

function Normalize-ListValue {
  param([object]$Value)
  if ($null -eq $Value) {
    return @()
  }

  if ($Value -is [System.Array]) {
    return @($Value | Where-Object { $_ -ne '' })
  }

  return @($Value).Where({ $_ -ne '' })
}

function Update-StateCommon {
  param([pscustomobject]$State)

  if ($Batch) {
    $State.activeBatch = $Batch
  }
  if ($Checkpoint) {
    $State.lastCompletedCheckpoint = $Checkpoint
  }
  if ($NextAction) {
    $State.nextAction = $NextAction
  }
  if ($Mode) {
    $State.mode = $Mode
  }
  if ($TelemetryStatus) {
    $State.telemetryStatus = $TelemetryStatus
  }
  if ($Notes) {
    $State.notes = Normalize-ListValue $State.notes + $Notes
  }
  if ($Blockers) {
    $State.blockers = Normalize-ListValue $State.blockers + $Blockers
  }
  $State.repoRoot = $repoRoot
  $State.updatedAtUtc = New-Timestamp
}

switch ($Action) {
  'start' {
    $startedAt = Get-Date
    $deadline = $startedAt.ToUniversalTime().AddHours($DurationHours)
    $state = [pscustomobject]@{
      repoRoot = $repoRoot
      status = 'active'
      startedAtUtc = $startedAt.ToUniversalTime().ToString('o')
      deadlineUtc = $deadline.ToString('o')
      durationHours = $DurationHours
      mode = $(if ($Mode) { $Mode } else { 'maintenance' })
      activeBatch = $Batch
      lastCompletedCheckpoint = ''
      nextAction = $NextAction
      notes = Normalize-ListValue $Notes
      blockers = Normalize-ListValue $Blockers
      telemetryStatus = $(if ($TelemetryStatus) { $TelemetryStatus } else { 'unknown' })
      updatedAtUtc = New-Timestamp
    }
    Write-State -Path $StatePath -State $state
    $state | ConvertTo-Json -Depth 6
    break
  }

  'checkpoint' {
    $state = Read-State -Path $StatePath
    if ($null -eq $state) {
      throw "No active state file found at $StatePath"
    }

    Update-StateCommon -State $state
    Write-State -Path $StatePath -State $state
    $state | ConvertTo-Json -Depth 6
    break
  }

  'finish' {
    $state = Read-State -Path $StatePath
    if ($null -eq $state) {
      throw "No active state file found at $StatePath"
    }

    $state.status = 'completed'
    Update-StateCommon -State $state
    Write-State -Path $StatePath -State $state
    $state | ConvertTo-Json -Depth 6
    break
  }

  'show' {
    $state = Read-State -Path $StatePath
    if ($null -eq $state) {
      throw "No active state file found at $StatePath"
    }

    $state | ConvertTo-Json -Depth 6
    break
  }
}

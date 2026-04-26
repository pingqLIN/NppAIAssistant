[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet('start', 'checkpoint', 'finish', 'show')]
  [string]$Action,

  [string]$Batch = '',
  [string]$Checkpoint = '',
  [string]$NextAction = '',
  [string]$Notes = '',
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
      activeBatch = $Batch
      lastCompletedCheckpoint = ''
      nextAction = $NextAction
      notes = @($Notes).Where({ $_ -ne '' })
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

    if ($Batch) {
      $state.activeBatch = $Batch
    }
    if ($Checkpoint) {
      $state.lastCompletedCheckpoint = $Checkpoint
    }
    if ($NextAction) {
      $state.nextAction = $NextAction
    }
    if ($Notes) {
      $existingNotes = @($state.notes)
      $state.notes = $existingNotes + $Notes
    }
    $state.updatedAtUtc = New-Timestamp
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
    if ($Checkpoint) {
      $state.lastCompletedCheckpoint = $Checkpoint
    }
    if ($NextAction) {
      $state.nextAction = $NextAction
    }
    if ($Notes) {
      $existingNotes = @($state.notes)
      $state.notes = $existingNotes + $Notes
    }
    $state.updatedAtUtc = New-Timestamp
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

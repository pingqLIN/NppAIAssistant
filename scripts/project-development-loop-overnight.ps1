[CmdletBinding()]
param(
  [string]$Batch = 'Overnight maintenance',
  [string]$NextAction = 'Resume from overnight report',
  [string]$RepoRoot = '',
  [int]$DurationHours = 8,
  [int]$HeartbeatMinutes = 30,
  [string]$TokenUsagePath = '',
  [string]$StatePath = '',
  [string]$HeartbeatPath = '',
  [switch]$NoLoop
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $RepoRoot) {
  $RepoRoot = (Get-Location).Path
}
$repoRoot = (Resolve-Path $RepoRoot).Path
if (-not $StatePath) {
  $StatePath = Join-Path $repoRoot '.codex\project-development-loop\active-run.json'
}
if (-not $HeartbeatPath) {
  $HeartbeatPath = Join-Path $repoRoot '.codex\project-development-loop\heartbeat.jsonl'
}
if ($TokenUsagePath -and -not [System.IO.Path]::IsPathRooted($TokenUsagePath)) {
  $TokenUsagePath = Join-Path $repoRoot $TokenUsagePath
}

function Ensure-ParentDirectory {
  param([string]$Path)
  $directory = Split-Path -Parent $Path
  if (-not (Test-Path $directory)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
  }
}

function Read-JsonFile {
  param([string]$Path)
  if (-not (Test-Path $Path)) {
    return $null
  }

  return Get-Content $Path -Raw | ConvertFrom-Json
}

function Get-TokenTelemetry {
  if (-not $TokenUsagePath) {
    return [pscustomobject]@{
      status = 'unavailable'
      blocker = 'machine-level token telemetry path not configured'
      data = $null
    }
  }

  if (-not (Test-Path $TokenUsagePath)) {
    return [pscustomobject]@{
      status = 'missing'
      blocker = "token telemetry file not found: $TokenUsagePath"
      data = $null
    }
  }

  try {
    $data = Get-Content $TokenUsagePath -Raw | ConvertFrom-Json
    return [pscustomobject]@{
      status = 'ok'
      blocker = ''
      data = $data
    }
  } catch {
    return [pscustomobject]@{
      status = 'invalid'
      blocker = "token telemetry file could not be parsed: $TokenUsagePath"
      data = $null
    }
  }
}

function Append-Heartbeat {
  param(
    [string]$Phase,
    [object]$State,
    [object]$Telemetry
  )

  Ensure-ParentDirectory -Path $HeartbeatPath

  $gitStatus = git -C $repoRoot status --short --branch
  $entry = [pscustomobject]@{
    timestampUtc = (Get-Date).ToUniversalTime().ToString('o')
    phase = $Phase
    repoRoot = $repoRoot
    activeBatch = $State.activeBatch
    lastCompletedCheckpoint = $State.lastCompletedCheckpoint
    nextAction = $State.nextAction
    telemetryStatus = $Telemetry.status
    telemetryBlocker = $Telemetry.blocker
    gitStatus = @($gitStatus)
  }

  ($entry | ConvertTo-Json -Compress) | Add-Content -LiteralPath $HeartbeatPath -Encoding UTF8
}

function Invoke-StateScript {
  param(
    [string]$Action,
    [string]$Checkpoint = '',
    [string]$Notes = '',
    [string]$Blockers = '',
    [string]$TelemetryStatus = ''
  )

  $arguments = @(
    '-NoProfile',
    '-ExecutionPolicy', 'Bypass',
    '-File', (Join-Path $PSScriptRoot 'project-development-loop-state.ps1'),
    '-RepoRoot', $repoRoot,
    '-Action', $Action,
    '-StatePath', $StatePath,
    '-Batch', $Batch,
    '-NextAction', $NextAction,
    '-Mode', 'overnight',
    '-DurationHours', [string]$DurationHours
  )

  if ($Checkpoint) {
    $arguments += @('-Checkpoint', $Checkpoint)
  }
  if ($Notes) {
    $arguments += @('-Notes', $Notes)
  }
  if ($Blockers) {
    $arguments += @('-Blockers', $Blockers)
  }
  if ($TelemetryStatus) {
    $arguments += @('-TelemetryStatus', $TelemetryStatus)
  }

  & powershell @arguments | Out-Null
}

$telemetry = Get-TokenTelemetry
Invoke-StateScript -Action 'start' -Notes 'Overnight supervisor initialized.' -Blockers $telemetry.blocker -TelemetryStatus $telemetry.status
$state = Read-JsonFile -Path $StatePath
Append-Heartbeat -Phase 'start' -State $state -Telemetry $telemetry

if ($NoLoop) {
  $state | ConvertTo-Json -Depth 6
  exit 0
}

$deadlineUtc = [datetime]::Parse($state.deadlineUtc)
while ((Get-Date).ToUniversalTime() -lt $deadlineUtc) {
  Start-Sleep -Seconds ([Math]::Max(60, $HeartbeatMinutes * 60))
  $telemetry = Get-TokenTelemetry
  Invoke-StateScript -Action 'checkpoint' -Checkpoint 'overnight heartbeat' -Notes 'Heartbeat recorded by overnight supervisor.' -Blockers $telemetry.blocker -TelemetryStatus $telemetry.status
  $state = Read-JsonFile -Path $StatePath
  Append-Heartbeat -Phase 'heartbeat' -State $state -Telemetry $telemetry
}

$telemetry = Get-TokenTelemetry
Invoke-StateScript -Action 'finish' -Checkpoint 'overnight supervisor window ended' -Notes 'Supervisor reached configured deadline.' -Blockers $telemetry.blocker -TelemetryStatus $telemetry.status
$state = Read-JsonFile -Path $StatePath
Append-Heartbeat -Phase 'finish' -State $state -Telemetry $telemetry
$state | ConvertTo-Json -Depth 6

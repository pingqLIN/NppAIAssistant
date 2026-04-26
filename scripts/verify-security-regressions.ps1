[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$mainPath = Join-Path $repoRoot 'src\NppAIAssistant.cpp'
$httpClientPath = Join-Path $repoRoot 'src\shared\HttpClient.cpp'
$securityChecklistPath = Join-Path $repoRoot 'docs\SECURITY_VERIFICATION.md'

$mainSource = Get-Content $mainPath -Raw
$httpClientSource = Get-Content $httpClientPath -Raw
$securityChecklist = Get-Content $securityChecklistPath -Raw

$checks = @(
  @{
    Name = 'Provider keys are loaded on demand'
    Passed = $mainSource -match 'SecureStorage::loadApiKey\(kOpenAIKeyName\)' -and
             $mainSource -match 'SecureStorage::loadApiKey\(kGeminiKeyName\)' -and
             $mainSource -match 'SecureStorage::loadApiKey\(kClaudeKeyName\)'
  }
  @{
    Name = 'Provider keys are not loaded during global config bootstrap'
    Passed = $mainSource -notmatch 'g_config\.openAIKey = SecureStorage::loadApiKey' -and
             $mainSource -notmatch 'g_config\.geminiKey = SecureStorage::loadApiKey' -and
             $mainSource -notmatch 'g_config\.claudeKey = SecureStorage::loadApiKey'
  }
  @{
    Name = 'Saved global config wipes transient provider keys'
    Passed = $mainSource -match 'wipeConfigSecrets\(g_config\);'
  }
  @{
    Name = 'HTTP URL sanitizer redacts query-string API keys'
    Passed = $httpClientSource -match 'sanitized\.replace\(valueStart, valueEnd - valueStart, L"<redacted>"\);'
  }
  @{
    Name = 'Security checklist references the regression script'
    Passed = $securityChecklist -match 'verify-security-regressions\.ps1'
  }
)

$failedChecks = @($checks | Where-Object { -not $_.Passed })

if ($failedChecks.Count -gt 0) {
  $failedChecks | ForEach-Object {
    Write-Error "FAILED: $($_.Name)"
  }
  exit 1
}

$checks | ForEach-Object {
  Write-Host "PASS: $($_.Name)"
}

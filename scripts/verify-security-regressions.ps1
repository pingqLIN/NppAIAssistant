[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$mainPath = Join-Path $repoRoot 'src\NppAIAssistant.cpp'
$httpClientPath = Join-Path $repoRoot 'src\shared\HttpClient.cpp'
$securityChecklistPath = Join-Path $repoRoot 'docs\SECURITY_VERIFICATION.md'
$packageScriptPath = Join-Path $repoRoot 'scripts\package-npp-ai-plugin.ps1'
$packageSmokeScriptPath = Join-Path $repoRoot 'scripts\smoke-package-install.ps1'

$mainSource = Get-Content $mainPath -Raw
$httpClientSource = Get-Content $httpClientPath -Raw
$securityChecklist = Get-Content $securityChecklistPath -Raw
$packageScript = Get-Content $packageScriptPath -Raw
$packageSmokeScript = Get-Content $packageSmokeScriptPath -Raw

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
  @{
    Name = 'Prompt templates are stored as non-secret settings'
    Passed = $mainSource -match 'SettingsStorage::saveString\(kIdentityTemplateName' -and
             $mainSource -match 'SettingsStorage::saveString\(kRulesTemplateName' -and
             $mainSource -match 'SettingsStorage::saveString\(kAssignmentTemplateName'
  }
  @{
    Name = 'OAuth token storage remains in secure storage'
    Passed = $mainSource -match 'SecureStorage::loadApiKey\(kCopilotOauthKeyName\)' -and
             $mainSource -match 'SecureStorage::saveApiKey\(kCopilotOauthKeyName'
  }
  @{
    Name = 'Prompt section builder keeps mandatory transparency section'
    Passed = $mainSource -match 'PromptSectionType::MandatorySystem' -and
             $mainSource -match 'L"Mandatory System"' -and
             $mainSource -match 'Do not assume hidden memory'
  }
  @{
    Name = 'Prompt preview labels token counts as estimates'
    Passed = $mainSource -match '\[Estimated Tokens\]' -and
             $mainSource -match 'estimated tokens'
  }
  @{
    Name = 'Prompt preview and send share the prompt section builder'
    Passed = $mainSource -match 'buildPromptSectionsForConfig\(config, getPromptPreviewUserRequest\(config\)\)' -and
             $mainSource -match 'renderPromptSections\(\s*buildPromptSectionsForConfig\(config, userPrompt, forceCodeOnlyOutput\)\)'
  }
  @{
    Name = 'Display scale is stored as a non-secret bounded setting'
    Passed = $mainSource -match 'kDisplayScalePercentName' -and
             $mainSource -match 'clampDisplayScalePercent' -and
             $mainSource -match 'SettingsStorage::saveString\(kDisplayScalePercentName'
  }
  @{
    Name = 'AI provider requests complete through UI-thread message dispatch'
    Passed = $mainSource -match 'WM_AI_REQUEST_COMPLETE' -and
             $mainSource -match 'std::thread' -and
             $mainSource -match 'PostMessageW\(targetPanel, WM_AI_REQUEST_COMPLETE' -and
             $mainSource -match 'completeAiRequest'
  }
  @{
    Name = 'Waiting animation is timer-driven while request is in progress'
    Passed = $mainSource -match 'kRequestAnimationTimerId' -and
             $mainSource -match 'getWaitingAnimationText' -and
             $mainSource -match 'SetTimer\(g_panel, kRequestAnimationTimerId'
  }
  @{
    Name = 'Memory storage is explicit, bounded, and non-secret'
    Passed = $mainSource -match 'kMemoryEnabledName' -and
             $mainSource -match 'kMaxMemoryChars' -and
             $mainSource -match 'PromptSectionType::Memory' -and
             $mainSource -match 'SettingsStorage::saveString\(kMemoryContentName'
  }
  @{
    Name = 'Custom context templates are bounded to three non-secret slots'
    Passed = $mainSource -match 'kContextTemplateCount = 3' -and
             $mainSource -match 'kAiContextCustomTemplateBase' -and
             $mainSource -match 'ContextTemplatesDlgProc' -and
             $mainSource -match 'SettingsStorage::saveString\(kContextTemplatePromptNames\[i\]'
  }
  @{
    Name = 'Plugins Admin packaging requires a direct HTTPS zip URL'
    Passed = $packageScript -match 'Test-PluginsAdminReleaseUrl' -and
             $packageScript -match 'ReleaseUrl must use HTTPS' -and
             $packageScript -match 'ReleaseUrl must point directly to the packaged \.zip asset' -and
             $packageScript -match 'pluginsAdminReady'
  }
  @{
    Name = 'Package smoke validates root DLL docs symbols and hash'
    Passed = $packageSmokeScript -match 'RootDllPresent' -and
             $packageSmokeScript -match 'RequiredDocsPresent' -and
             $packageSmokeScript -match 'PdbPresent' -and
             $packageSmokeScript -match 'Package SHA-256 mismatch' -and
             $securityChecklist -match 'smoke-package-install\.ps1'
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

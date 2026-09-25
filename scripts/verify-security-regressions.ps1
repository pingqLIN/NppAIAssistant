[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$mainPath = Join-Path $repoRoot 'src\NppAIAssistant.cpp'
$httpClientPath = Join-Path $repoRoot 'src\shared\HttpClient.cpp'
$llmApiClientPath = Join-Path $repoRoot 'src\shared\LLMApiClient.cpp'
$structuredOutputPath = Join-Path $repoRoot 'src\shared\StructuredOutput.cpp'
$promptGovernancePath = Join-Path $repoRoot 'src\shared\PromptGovernance.cpp'
$promptGovernanceHeaderPath = Join-Path $repoRoot 'src\shared\PromptGovernance.h'
$resourcePath = Join-Path $repoRoot 'src\NppAIAssistantResources.rc'
$securityChecklistPath = Join-Path $repoRoot 'docs\SECURITY_VERIFICATION.md'
$packageScriptPath = Join-Path $repoRoot 'scripts\package-npp-ai-plugin.ps1'
$packageSmokeScriptPath = Join-Path $repoRoot 'scripts\smoke-package-install.ps1'

$mainSource = Get-Content $mainPath -Raw
$httpClientSource = Get-Content $httpClientPath -Raw
$llmApiClientSource = Get-Content $llmApiClientPath -Raw
$structuredOutputSource = Get-Content $structuredOutputPath -Raw
$promptGovernanceSource = Get-Content $promptGovernancePath -Raw
$promptGovernanceHeader = Get-Content $promptGovernanceHeaderPath -Raw
$resourceSource = Get-Content $resourcePath -Raw
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
    Name = 'Prompt policy is explicit, minimal, and distinct from runtime enforcement'
    Passed = $mainSource -match 'PromptSectionType::MandatorySystem' -and
             $mainSource -match 'L"Prompt Policy"' -and
             $mainSource -match 'buildPromptPolicyText' -and
             $promptGovernanceSource -match 'one independent request' -and
             $promptGovernanceSource -match 'Do not claim to have used tools'
  }
  @{
    Name = 'Prompt preview labels token counts as estimates'
    Passed = $mainSource -match '\[Estimated Tokens\]' -and
             $mainSource -match 'estimated tokens'
  }
  @{
    Name = 'Prompt preview and Composer share the context-aware prompt builder'
    Passed = $mainSource -match 'buildPromptAssemblyResult\(\s*config, context, getPromptPreviewUserRequest\(config, context\)\)' -and
             $mainSource -match 'buildPromptAssemblyResult\(\s*draft\.config, draft\.assemblyContext, draft\.finalPrompt' -and
             $mainSource -match 'request\.effectivePrompt\s*=\s*draft\.assembledPrompt'
  }
  @{
    Name = 'Prompt editors render Win32 CRLF while requests retain canonical LF'
    Passed = $mainSource -match 'normalizePromptLineEndings' -and
             $mainSource -match 'toWin32EditText' -and
             $mainSource -match 'section\.content = normalizePromptLineEndings\(section\.content\)' -and
             $mainSource -match 'draft\.userPrompt = normalizePromptLineEndings\(input\.taskInstruction\)' -and
             $mainSource -match 'draft\.sourceContext = normalizePromptLineEndings\(input\.sourceContext\)' -and
             $mainSource -match 'canonicalIntended =\s*normalizePromptLineEndings\(intendedText\)' -and
             $mainSource -match 'value = normalizePromptLineEndings\(text\)' -and
             $mainSource -match 'SetWindowTextW\(previewEdit, editText\.c_str\(\)\)' -and
             $mainSource -match 'SetWindowTextW\(editor, editText\.c_str\(\)\)'
  }
  @{
    Name = 'Composer enforces canonical 1 MiB with CRLF display headroom'
    Passed = $mainSource -match 'kPromptComposerEditMaxChars = kPromptComposerMaxChars \* 2' -and
             $mainSource -match 'static_cast<WPARAM>\(kPromptComposerEditMaxChars\)' -and
             $mainSource -match 'draft->finalPrompt\.size\(\) <= kPromptComposerMaxChars' -and
             $mainSource -match 'static_cast<size_t>\(editLength\) < kPromptComposerEditMaxChars' -and
             $mainSource -match 'actualText\.size\(\) > kPromptComposerMaxChars'
  }
  @{
    Name = 'Stable prompt prefix preserves exact concatenation invariant'
    Passed = $mainSource -match 'struct PromptAssemblyResult' -and
             $mainSource -match 'result\.dynamicSuffix = result\.fullPrompt\.substr\(result\.stablePrefix\.size\(\)\)' -and
             $mainSource -match 'cacheEligible'
  }
  @{
    Name = 'Governance harness keeps stable modules first and separates dynamic request input'
    Passed = $mainSource -match 'struct PromptRequestInput' -and
             $mainSource -match 'std::wstring taskInstruction' -and
             $mainSource -match 'std::wstring sourceContext' -and
             $mainSource -match '(?s)PromptSectionId::Assignment.*PromptSectionId::ScenarioModules.*PromptSectionId::OutputContract.*PromptSectionId::Memory.*PromptSectionId::RuntimeContext.*PromptSectionId::TaskInstruction.*PromptSectionId::SourceContext'
  }
  @{
    Name = 'Compatible providers are integrated into Settings with persisted defaults'
    Passed = $resourceSource -match 'IDC_SETTINGS_TAB' -and
             $resourceSource -match 'IDC_COMPATIBLE_DEFAULT_MODEL_COMBO' -and
             $resourceSource -match 'IDC_LMSTUDIO_DEFAULT_MODEL_COMBO' -and
             $resourceSource -notmatch 'IDD_AIASSISTANT_COMPATIBLE_SERVICES\s+DIALOGEX' -and
             $resourceSource -notmatch 'IDC_COMPATIBLE_SERVICES_BUTTON' -and
             $mainSource -match 'kCompatibleDefaultModelName' -and
             $mainSource -match 'kLmStudioDefaultModelName' -and
             $mainSource -match 'SettingsStorage::saveString\(kCompatibleDefaultModelName' -and
             $mainSource -match 'SettingsStorage::saveString\(kLmStudioDefaultModelName'
  }
  @{
    Name = 'Settings child dialogs remain draft-only until the outer OK'
    Passed = $mainSource -notmatch 'savePreferencesToSettings\(\*config\)' -and
             $mainSource -match 'if \(::DialogBoxParamW\(g_hInst, MAKEINTRESOURCEW\(IDD_AIASSISTANT_SETTINGS\)' -and
             $mainSource -match 'saveConfig\(edited\);'
  }
  @{
    Name = 'Compatible model discovery rejects stale endpoint results'
    Passed = $mainSource -match 'std::wstring endpointSnapshot' -and
             $mainSource -match 'currentEndpoint != result\.endpointSnapshot' -and
             $mainSource -match 'isCurrentModelDiscovery\(hwnd, \*result\)' -and
             $mainSource -match 'successfulCompatibleEndpoint' -and
             $mainSource -match 'successfulLmStudioEndpoint'
  }
  @{
    Name = 'Compatible discovery freezes credential-dependent inputs while active'
    Passed = $mainSource -match 'setSettingsDiscoveryControlsEnabled\(HWND hwnd, LLMProvider provider' -and
             $mainSource -match 'IDC_COMPATIBLE_API_KEY_EDIT' -and
             $mainSource -match 'IDC_LMSTUDIO_API_KEY_EDIT' -and
             $mainSource -match 'setSettingsDiscoveryControlsEnabled\(hwnd, provider, false\)' -and
             $mainSource -match 'setSettingsDiscoveryControlsEnabled\(hwnd, result\.provider, true\)'
  }
  @{
    Name = 'Unavailable saved defaults are preserved until explicit selection'
    Passed = $mainSource -match 'compatibleExplicitSelectionEndpoint' -and
             $mainSource -match 'lmStudioExplicitSelectionEndpoint' -and
             $mainSource -match 'originalCompatibleDefaultModel' -and
             $mainSource -match 'originalLmStudioDefaultModel' -and
             $mainSource -match 'CompatibleSavedModelUnavailable' -and
             $mainSource -match 'CBN_SELENDOK'
  }
  @{
    Name = 'Explicit model selection is scoped to the discovered endpoint'
    Passed = $mainSource -match 'compatibleExplicitSelectionEndpoint == compatibleUrl' -and
             $mainSource -match 'lmStudioExplicitSelectionEndpoint == lmStudioUrl' -and
             $mainSource -match 'state\.compatibleExplicitSelectionEndpoint\.clear\(\)' -and
             $mainSource -match 'state\.lmStudioExplicitSelectionEndpoint\.clear\(\)' -and
             $mainSource -match 'state->successfulCompatibleEndpoint' -and
             $mainSource -match 'state->successfulLmStudioEndpoint'
  }
  @{
    Name = 'Invalid model controls cannot become a persisted placeholder'
    Passed = $mainSource -match 'ch >= 0x7F && ch <= 0x9F' -and
             $llmApiClientSource -match 'ch >= 0x7F && ch <= 0x9F' -and
             $mainSource -match 'bool populateSettingsDefaultModelCombo' -and
             $mainSource -match 'if \(!populateSettingsDefaultModelCombo' -and
             $mainSource -match 'CompatibleStatusUnavailable\)\);\s*return;' -and
             $mainSource -match 'return isAcceptableModelId\(value\);'
  }
  @{
    Name = 'Prompt Sections exposes stable IDs with editable and locked policies'
    Passed = $mainSource -match 'enum class PromptSectionId' -and
             $mainSource -match 'isEditablePromptSection' -and
             $mainSource -match 'EM_SETREADONLY, editable \? FALSE : TRUE' -and
             $mainSource -match 'PromptSectionId::MandatorySystem' -and
             $mainSource -match 'PromptSectionId::SourceContext' -and
             $mainSource -match 'PromptSectionLockedHint'
  }
  @{
    Name = 'Built-in prompt sections require explicit unlock before editing'
    Passed = $promptGovernanceHeader -match 'struct PromptSectionLockState' -and
             $promptGovernanceHeader -match 'bool promptPolicyLocked = true' -and
             $promptGovernanceHeader -match 'bool identityLocked = true' -and
             $promptGovernanceHeader -match 'bool outputContractLocked = true' -and
             $mainSource -match 'isPromptSectionLockable' -and
             $mainSource -match 'setPromptSectionLocked' -and
             $mainSource -match 'kPromptPolicyLockedName' -and
             $resourceSource -match 'IDC_PROMPT_SECTION_LOCK_CHECK'
  }
  @{
    Name = 'Prompt Section templates are bounded before settings persistence'
    Passed = $promptGovernanceHeader -match 'kMaxPromptSectionTemplateChars = 30000' -and
             $mainSource -match 'EM_SETLIMITTEXT' -and
             $mainSource -match 'promptSectionTemplatesFitSettingsStorage' -and
             $mainSource -match 'if \(!promptSectionTemplatesFitSettingsStorage\(config\)\) return false;' -and
             $mainSource -match 'SettingsStorage::beginWriteBatch\(\);' -and
             $mainSource -match 'if \(!SettingsStorage::endWriteBatch\(\)\) return false;' -and
             $mainSource -match 'return SettingsStorage::saveSchemaVersion\(kSettingsSchemaVersion\);' -and
             $mainSource -match 'if \(savePreferencesToSettings\(g_config\)\) \{\s*cleanupSecurePreferenceBlobs\(\);' -and
             $mainSource -match 'if \(!savePreferencesToSettings\(config\)\) return;\s*SecureStorage::saveApiKey' -and
             $mainSource -match 'promptSectionTemplateStorageLimitMessage'
  }
  @{
    Name = 'Model selection is bounded dynamic text and preserves configured defaults'
    Passed = $mainSource -match 'kMaxModelIdChars = 8192' -and
             $mainSource -match 'isAcceptableModelId\(model\)' -and
             $mainSource -match 'getConfiguredDefaultModel' -and
             $mainSource -match 'currentIndex >= 0 \? currentIndex' -and
             $mainSource -match 'g_currentModel\.clear\(\);\s*(?:updateCompactProviderName\(\);\s*)?updateModelCombo\(\);'
  }
  @{
    Name = 'Manual or mismatched Composer text clears cache prefix'
    Passed = $mainSource -match 'draft->assembledStablePrefix\.clear\(\);' -and
             $mainSource -match 'if \(draft\.hasEditorInputError\) draft\.assembledStablePrefix\.clear\(\);' -and
             $mainSource -match 'request\.cacheStablePrefix = draft\.assembledStablePrefix'
  }
  @{
    Name = 'Context menu modifier is wired persisted and only gates mouse invocation'
    Passed = $mainSource -match 'kContextMenuModifierName' -and
             $mainSource -match 'populateContextMenuModifierCombo\(' -and
             $mainSource -match 'config\.contextMenuModifier = sanitizeContextMenuModifier\(modifierSelection\)' -and
             $mainSource -match 'isConfiguredContextModifierPressed' -and
             $mainSource -match 'isMouseInvocation &&\s*isConfiguredContextModifierPressed' -and
             $mainSource -match 'lParam != static_cast<LPARAM>\(-1\)'
  }
  @{
    Name = 'Provider numeric values preserve legacy values and add compatible services'
    Passed = $mainSource -match 'OpenAI = 0, Gemini = 1, Claude = 2, Copilot = 3' -and
             $mainSource -match 'OpenAICompatible = 4, LMStudio = 5'
  }
  @{
    Name = 'Compatible URLs require HTTPS or exact loopback and reject endpoint paths'
    Passed = $mainSource -match 'normalizeCompatibleBaseUrl' -and
             $mainSource -match 'if \(authority\.empty\(\)\) return false;' -and
             $mainSource -match 'scheme != L"https"' -and
             $mainSource -match 'strictLoopback' -and
             $mainSource -match 'kLoopbackPrefix' -and
             $mainSource -match 'L"/chat/completions"'
  }
  @{
    Name = 'Test Default Connection reaches the selected compatible service'
    Passed = $mainSource -match 'case LLMProvider::OpenAICompatible:\s*\{' -and
             $mainSource -match 'compatibleBaseUrl, apiKey, compatibleLoopback' -and
             $mainSource -match 'CompatibleTrustWarning'
  }
  @{
    Name = 'LM Studio discovery uses fixed loopback no-proxy bounded requests'
    Passed = $mainSource -match 'L"http://127\.0\.0\.1:1234/v1"' -and
             $llmApiClientSource -match 'kLoopbackDiscoveryTimeoutMs = 1500' -and
             $llmApiClientSource -match 'loopback \? kLoopbackDiscoveryTimeoutMs : 0, loopback' -and
             $httpClientSource -match 'WINHTTP_ACCESS_TYPE_NO_PROXY'
  }
  @{
    Name = 'Loopback generation timeout is distinct from fast model discovery'
    Passed = $llmApiClientSource -match 'kLoopbackGenerationTimeoutMs = 900 \* 1000' -and
             $llmApiClientSource -match 'kLoopbackDiscoveryTimeoutMs = 1500' -and
             $llmApiClientSource -match 'loopback \? kLoopbackGenerationTimeoutMs : 0, loopback' -and
             $llmApiClientSource -match 'Compatible service request failed\.";[\s\S]*sanitizeUntrustedProviderError'
  }
  @{
    Name = 'LM Studio discovery is asynchronous localized and stale-result guarded'
    Passed = $mainSource -match 'WM_AI_MODEL_DISCOVERY_COMPLETE' -and
             $mainSource -match 'startCompatibleModelDiscovery' -and
             $mainSource -match 'PostMessageW\(target,\s*WM_AI_MODEL_DISCOVERY_COMPLETE' -and
             $mainSource -match 'g_modelDiscoveryResults\[generation\] = std::move\(result\)' -and
             $mainSource -match 'takeModelDiscoveryResult' -and
             $mainSource -match 'kModelDiscoveryGenerationProperty' -and
             $mainSource -match 'isCurrentModelDiscovery' -and
             $mainSource -match 'applyLocalizedSettingsText' -and
             $mainSource -match 'completeSettingsModelDiscovery' -and
             $mainSource -notmatch 'reinterpret_cast<ModelDiscoveryResult \*>'
  }
  @{
    Name = 'LM Studio port parsing consumes the complete numeric value'
    Passed = $mainSource -match 'std::stoi\(portText, &parsed\)' -and
             $mainSource -match 'parsed == portText\.size\(\)'
  }
  @{
    Name = 'Compatible responses use raw output parsing and redact service errors'
    Passed = $structuredOutputSource -match 'extractResponsesEnvelope' -and
             $structuredOutputSource -match 'stringProperty\(part, L"text"\)' -and
             $llmApiClientSource -match 'extractResponsesEnvelope\(httpResponse\.body\)' -and
             $llmApiClientSource -match 'Compatible service request failed\.' -and
             $llmApiClientSource -notmatch 'response\.errorMessage \+= L"\\n" \+ detail'
  }
  @{
    Name = 'Compatible requests explicitly select bounded non-streaming responses'
    Passed = $llmApiClientSource -match 'store\\\":false,\\\"stream\\\":false' -and
             $llmApiClientSource -match '\\\"messages\\\":\[\{\\\"role\\\":\\\"user\\\"' -and
             $llmApiClientSource -match '\\\"stream\\\":false'
  }
  @{
    Name = 'Chat completion parsing is scoped to choices and message content'
    Passed = $structuredOutputSource -match 'findProperty\(parsed\.value, L"choices"\)' -and
             $structuredOutputSource -match 'findProperty\(choice, L"message"\)' -and
             $structuredOutputSource -match 'stringProperty\(choice, L"text"\)' -and
             $llmApiClientSource -match 'extractChatCompletionEnvelope\(httpResponse\.body\)'
  }
  @{
    Name = 'Default output contract excludes provider transport envelopes'
    Passed = $mainSource -match 'Return the assistant answer itself, not a provider transport' -and
             $mainSource -match 'Do not wrap the answer in fields such as choices, message' -and
             $mainSource -match 'return exactly one valid' -and
             $mainSource -match 'JSON value without markdown fences or extra commentary'
  }
  @{
    Name = 'Harness v3 migrates only recognized default templates'
    Passed = $mainSource -match 'kPromptHarnessVersion = 3' -and
             $mainSource -match 'getLegacyDefaultIdentityTemplateV2' -and
             $mainSource -match 'getLegacyDefaultRulesTemplateV2' -and
             $mainSource -match 'getLegacyDefaultAssignmentTemplateV2' -and
             $mainSource -match 'savedDefaultTemplates \|\| storedHarnessVersion < kPromptHarnessVersion'
  }
  @{
    Name = 'Composer edits task instructions while runtime policy remains assembled'
    Passed = $mainSource -match 'Task Instructions \(editable\)' -and
             $mainSource -match 'Runtime Policy \(read-only\)' -and
             $mainSource -match 'request\.userPrompt = draft\.finalPrompt' -and
             $mainSource -match 'request\.effectivePrompt = draft\.assembledPrompt' -and
             $mainSource -match 'makeBuiltInStructuredOutputConfig'
  }
  @{
    Name = 'Untrusted source and memory data are delimited and manifest excludes payloads'
    Passed = $mainSource -match 'wrapUntrustedPromptData\(L"source_context", sourceContext\)' -and
             $mainSource -match 'wrapUntrustedPromptData\(L"memory", trimmedMemory\)' -and
             $mainSource -match 'buildPromptAssemblyManifest' -and
             $promptGovernanceSource -match 'isSafeElementName' -and
             $promptGovernanceSource -match 'escapeClosingElementTag' -and
             $promptGovernanceSource -match 'L" trust=\\"untrusted\\">\\n"' -and
             $promptGovernanceSource -match 'characterCount' -and
             $promptGovernanceSource -notmatch 'rawContent'
  }
  @{
    Name = 'Claude explicit cache metadata is capability gated'
    Passed = $llmApiClientSource -match 'isKnownClaudePromptCacheModel' -and
             $llmApiClientSource -match 'isKnownClaudePromptCacheModel\(model\) && !cacheStablePrefix\.empty\(\)'
  }
  @{
    Name = 'Compatible provider API keys remain DPAPI-backed and request endpoint is snapshotted'
    Passed = $mainSource -match 'kCompatibleApiKeyName' -and
             $mainSource -match 'kLmStudioApiKeyName' -and
             $mainSource -match 'SecureStorage::saveApiKey\(kCompatibleApiKeyName' -and
             $mainSource -match 'request\.compatibleBaseUrl = draft\.config\.compatibleBaseUrl' -and
             $mainSource -match 'request\.providerApiKey = getProviderApiKey\(request\.provider\)' -and
             $mainSource -match 'std::wstring &apiKey = request\.providerApiKey'
  }
  @{
    Name = 'Display scale is stored as a non-secret bounded setting'
    Passed = $mainSource -match 'kDisplayScalePercentName' -and
             $mainSource -match 'clampDisplayScalePercent' -and
             $mainSource -match 'SettingsStorage::saveString\(kDisplayScalePercentName'
  }
  @{
    Name = 'Provider names stay bounded and panel layout uses DPI-aware geometry'
    Passed = $mainSource -match 'kMaxCompatibleDisplayNameChars = 128' -and
             $mainSource -match 'kCompactProviderHeaderFormatChars = 64' -and
             $mainSource -match 'kCompactProviderHeaderSingleLineSafetyInset = 8' -and
             $mainSource -match 'const int safeTextWidth = std::max' -and
             $mainSource -match 'sanitizeCompatibleDisplayName' -and
             $mainSource -match 'isHighSurrogate' -and
             $mainSource -match 'isLowSurrogate' -and
             $mainSource -match 'truncateUtf16AtScalarBoundary' -and
             $mainSource -match 'GetDpiForWindow\(g_panel\)' -and
             $mainSource -match 'PanelLayout::compute' -and
             $mainSource -match 'metrics.preview = measuredWidth' -and
             $mainSource -match 'WM_DPICHANGED_AFTERPARENT' -and
             $mainSource -match 'IDC_COMPATIBLE_DISPLAY_NAME_EDIT\),\s*EM_SETLIMITTEXT' -and
             $mainSource -match 'updateModelCombo\(\);\s*resizePanelControls\(\);' -and
             $mainSource -match 'pass < \(needsFallbackFormatting \? 2 : 1\)' -and
             $mainSource -match 'if \(firstWidth > safeTextWidth \|\| secondWidth > safeTextWidth\) \{\s*split = next;\s*continue;' -and
             $mainSource -match 'measure\(measured\) > safeTextWidth' -and
             $mainSource -match 'formattedName = first \+ L"\\r\\n" \+ second;' -and
             $mainSource -match 'Every measurement above completes while this DC/font selection is valid\.\s*if \(previous\) ::SelectObject\(dc, previous\);\s*::ReleaseDC\(providerName, dc\);'
  }
  @{
    Name = 'AI provider requests complete through UI-thread message dispatch'
    Passed = $mainSource -match 'WM_AI_REQUEST_COMPLETE' -and
             $mainSource -match 'std::thread' -and
             $mainSource -match 'PostMessageW\(targetPanel, WM_AI_REQUEST_COMPLETE' -and
             $mainSource -match 'completeAiRequest'
  }
  @{
    Name = 'Request feedback uses a dedicated status control without chat redraw animation'
    Passed = $mainSource -match 'IDC_AI_REQUEST_STATUS_STATIC' -and
             $mainSource -match 'RequestProgressState' -and
             $mainSource -match 'updateRequestStatusDisplay' -and
             $mainSource -notmatch 'getWaitingAnimationText' -and
             $mainSource -match 'SPI_GETCLIENTAREAANIMATION' -and
             $mainSource -match 'WorkbenchFormat::animate' -and
             $mainSource -match '(?s)if \(wParam == kRequestAnimationTimerId\) \{\s*\+\+g_animationTick;\s*setRequestProgress\(g_requestProgress\);\s*return TRUE;'
  }
  @{
    Name = 'HTTP transport telemetry excludes request secrets and payloads'
    Passed = $httpClientSource -match 'HttpTransportPhase' -and
             $httpClientSource -match 'setThreadTransportObserver' -and
             $httpClientSource -match 'notifyTransportPhase' -and
             $httpClientSource -notmatch 'TransportObserver.*Authorization' -and
             $mainSource -match 'WM_AI_REQUEST_TRANSPORT'
  }
  @{
    Name = 'Preview bypass reuses the assembled request and preserves structured transport'
    Passed = $mainSource -match 'showPromptPreviewBeforeSend' -and
             $mainSource -match 'buildAiRequestFromDraft' -and
             $mainSource -match 'makeBuiltInStructuredOutputConfig' -and
             $mainSource -match 'if \(draft\.config\.showPromptPreviewBeforeSend\)' -and
             $mainSource -match 'buildPromptAssemblyResult'
  }
  @{
    Name = 'Conversation branches remain local and single-turn by default'
    Passed = $mainSource -match 'LocalConversationSession' -and
             $mainSource -match 'kMaxLocalConversationCount' -and
             $mainSource -match 'switchLocalConversation' -and
             $mainSource -match 'Previous messages are not sent to the provider automatically' -and
             $mainSource -match 'Branch conversation' -and
             $mainSource -match 'New conversation'
  }
  @{
    Name = 'Prompt composition order is persisted and constrained by policy before untrusted data'
    Passed = $promptGovernanceSource -match 'PromptCompositionOrder' -and
             $promptGovernanceSource -match 'CacheStablePolicyFirst' -and
             $mainSource -match 'applyPromptCompositionOrder' -and
             $mainSource -match 'All governance and output' -and
             $mainSource -match 'kPromptCompositionOrderName'
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
  @{
    Name = 'Structured JSON persists provider-neutral settings without migration version bump'
    Passed = $mainSource -match 'kOutputModeName' -and
             $mainSource -match 'kStructuredSchemaPresetName' -and
             $mainSource -match 'kStructuredStrictName' -and
             $mainSource -match 'kStructuredValidateResponseName' -and
             $mainSource -match 'kSettingsSchemaVersion = 1'
  }
  @{
    Name = 'Structured schemas use endpoint-specific transport and never enter prompt assembly'
    Passed = $llmApiClientSource -match 'appendStructuredOutputFormat' -and
             $llmApiClientSource -match 'CompatibleApiMode::Responses' -and
             $structuredOutputSource -match 'buildStructuredOutputFormat' -and
             $structuredOutputSource -match 'response_format' -and
             $structuredOutputSource -match 'text.*format' -and
             $mainSource -match 'do not repeat a schema in this prompt' -and
             $mainSource -match 'makeBuiltInStructuredOutputConfig'
  }
  @{
    Name = 'Unknown providers block Structured JSON rather than silently falling back'
    Passed = $mainSource -match 'StructuredOutputSupport::Unknown' -and
             $mainSource -match 'ResponseFailure::UnsupportedStructuredOutput' -and
             $mainSource -match 'Structured JSON is not verified for this provider'
  }
  @{
    Name = 'Structured response validation is fail-closed and typed'
    Passed = $structuredOutputSource -match 'Unsupported schema keyword:' -and
             $structuredOutputSource -match 'ResponseFailure::JsonParseError' -and
             $structuredOutputSource -match 'ResponseFailure::SchemaValidationError' -and
             $structuredOutputSource -match 'ResponseFailure::GenerationIncomplete' -and
             $structuredOutputSource -match 'additionalProperties' -and
             $structuredOutputSource -match 'status == L"cancelled"' -and
             $structuredOutputSource -match 'type"\) == L"refusal"' -and
             $llmApiClientSource -match 'if \(structuredOutput\.enabled\) \{' -and
             $llmApiClientSource -notmatch 'structuredOutput\.enabled && structuredOutput\.validateResponse'
  }
  @{
    Name = 'Structured failures retain content but cannot replace editor selection'
    Passed = $mainSource -match 'Raw model content retained for review' -and
             $mainSource -match 'result->replaceSelection && result->response\.success' -and
             $mainSource -match 'draft\.replaceSelection && !request\.structuredOutput\.enabled'
  }
  @{
    Name = 'Structured early exits wipe request keys and reject malformed Unicode transport'
    Passed = $mainSource -match 'RequestApiKeyWiper' -and
             $mainSource -match '~RequestApiKeyWiper\(\) \{ wipeString\(value\); \}' -and
             $httpClientSource -match 'MB_ERR_INVALID_CHARS' -and
             $httpClientSource -match 'WC_ERR_INVALID_CHARS' -and
             $httpClientSource -match 'utf8\.data\(\), static_cast<int>\(utf8\.size\(\)\)' -and
             $httpClientSource -match 'wide\.data\(\), static_cast<int>\(wide\.size\(\)\)'
  }
  @{
    Name = 'Provider errors are redacted and invalid request Unicode never reaches HTTP'
    Passed = ([regex]::Matches(
                  $llmApiClientSource,
                  'sanitizeUntrustedProviderError')).Count -ge 8 -and
             $llmApiClientSource -match 'redactValue\(message, knownApiKey\)[\s\S]*sanitized\.resize\(1024\)' -and
             $httpClientSource -match '!body\.empty\(\) && utf8Body\.empty\(\)' -and
             $httpClientSource -match 'HTTP request body contains invalid Unicode'
  }
  @{
    Name = 'Detached AI request completion is generation-owned and pointer-free'
    Passed = $mainSource -match 'g_requestGeneration' -and
             $mainSource -match 'g_aiRequestResultMutex' -and
             $mainSource -match 'g_aiRequestResults\[requestGeneration\] = std::move\(result\)' -and
             $mainSource -match 'PostMessageW\(targetPanel, WM_AI_REQUEST_COMPLETE,[\s\S]*static_cast<WPARAM>\(requestGeneration\), 0\)' -and
             $mainSource -notmatch 'reinterpret_cast<LPARAM>\(result\.release\(\)\)'
  }
  @{
    Name = 'Structured validation and Copilot diagnostics are sanitized and bounded'
    Passed = $llmApiClientSource -match 'sanitizeUntrustedProviderError\(validation\.errorMessage, knownApiKey\)' -and
             $llmApiClientSource -match 'return sanitizeUntrustedProviderError\(sanitized, L""\)' -and
             $llmApiClientSource -match 'return sanitizeUntrustedProviderError\(message, L""\)' -and
             $llmApiClientSource -match 'github_pat_'
  }
  @{
    Name = 'Invalid UTF-8 HTTP responses fail before provider parsing'
    Passed = ([regex]::Matches(
                  $httpClientSource,
                  '!responseBody\.empty\(\) && response\.body\.empty\(\)')).Count -eq 2 -and
             $httpClientSource -match 'HTTP response contains invalid UTF-8'
  }
  @{
    Name = 'Malformed provider strings and partial WinHTTP bodies fail closed'
    Passed = $llmApiClientSource -match 'bool closed = false' -and
             $llmApiClientSource -match 'if \(!closed\) return L""' -and
             ([regex]::Matches(
                  $httpClientSource,
                  'Failed to query HTTP response data')).Count -eq 2 -and
             ([regex]::Matches(
                  $httpClientSource,
                  'Failed to read HTTP response data')).Count -eq 2
  }
  @{
    Name = 'Authentication debug fails closed on credential-bearing fields'
    Passed = $llmApiClientSource -match 'sensitiveMarkers' -and
             $llmApiClientSource -match '<redacted authentication response>'
  }
  @{
    Name = 'Gemini Claude and Copilot success paths use scoped JSON envelopes'
    Passed = $structuredOutputSource -match 'extractGeminiEnvelope' -and
             $structuredOutputSource -match 'findProperty\(parsed\.value, L"candidates"\)' -and
             $structuredOutputSource -match 'extractClaudeEnvelope' -and
             $structuredOutputSource -match 'findProperty\(parsed\.value, L"content"\)' -and
             $llmApiClientSource -match 'applyEnvelopeResult\(response, extractGeminiEnvelope' -and
             $llmApiClientSource -match 'applyEnvelopeResult\(response, extractClaudeEnvelope' -and
             $llmApiClientSource -match 'applyEnvelopeResult\(response, extractChatCompletionEnvelope\(httpResponse\.body\)'
  }
  @{
    Name = 'Provider worker lifetime is owned through Notepad shutdown'
    Passed = $mainSource -match 'launchPluginWorker' -and
             $mainSource -match 'joinPluginWorkersForShutdown' -and
             $mainSource -match 'case NPPN_SHUTDOWN:' -and
             $mainSource -match 'workers\.swap\(g_pluginWorkers\)' -and
             $mainSource -match 'completed->store\(true, std::memory_order_release\)' -and
             $mainSource -match 'it->completed->load\(std::memory_order_acquire\)' -and
             $mainSource -match 'if \(worker\.thread\.joinable\(\)\) worker\.thread\.join\(\)' -and
             $mainSource -notmatch '\.detach\(\)'
  }
  @{
    Name = 'Gemini and Claude completion statuses fail closed'
    Passed = $structuredOutputSource -match 'finishReasonValue->scalar == L"STOP"' -and
             $structuredOutputSource -match 'unsupported finishReason' -and
             $structuredOutputSource -match 'stopReasonValue->scalar == L"end_turn"' -and
             $structuredOutputSource -match 'stopReasonValue->scalar == L"stop_sequence"' -and
             $structuredOutputSource -match 'unsupported stop_reason'
  }
  @{
    Name = 'Model discovery dispatch has bounded live-window fallback'
    Passed = $mainSource -match 'PostMessageW\(target,\s*WM_AI_MODEL_DISCOVERY_COMPLETE' -and
             $mainSource -match 'SendMessageTimeoutW\(target,\s*WM_AI_MODEL_DISCOVERY_COMPLETE' -and
             $mainSource -match 'SMTO_ABORTIFHUNG \| SMTO_BLOCK, 5000'
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

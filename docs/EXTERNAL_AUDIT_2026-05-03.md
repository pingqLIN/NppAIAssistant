# External Audit Report - 2026-05-03

## Audit Mode

same-provider-subagent.

Note: the first subagent attempt was blocked by the repo startup prompt gate.
The reviewers were resumed with `O` and then completed read-only audits. A
same-provider-local fallback was also recorded while waiting, but the completed
subagent findings below are the controlling review inputs.

## Scope

Reviewed scope:

- `docs/DEVELOPMENT_PLAN_2026-05-03.md`
- `src/NppAIAssistant.cpp`
- `src/NppAIAssistantResources.h`
- `src/NppAIAssistantResources.rc`
- `src/shared/SettingsStorage.*`
- `src/shared/SecureStorage.*`
- `scripts/verify-security-regressions.ps1`
- selected SidePilot reference files named in the development plan

## Reference Inputs

- `local-project`: `Q:\Projects\SidePilot\extension\sidepanel.js` - System Identity module token insertion and resolved preview pattern
- `local-project`: `Q:\Projects\SidePilot\extension\js\rules-manager.js` - rules template seed, source metadata, and reset/apply pattern
- `local-project`: `Q:\Projects\SidePilot\docs\SIDEPANEL_DESIGN.md` - compact operator-console UI direction
- `local-project`: `Q:\Projects\SidePilot\docs\FEATURES.md` - prompt injection layer ordering
- `skill`: `C:\Users\miles\.codex\skills\supabase-hosted-admin-oauth\SKILL.md` - OAuth readiness and validation boundary
- `skill`: `C:\Users\miles\.codex\skills\notepad-plus-plus-plugin-workflow\SKILL.md` - Notepad++ plugin release gate separation
- `skill`: `C:\Users\miles\.codex\skills\external-audit-orchestrator\references\report-format.md` - normalized audit report format

## Review 1 - OAuth, Storage, and Security

### Findings

1. Critical: OAuth token lifecycle is underspecified and over-relies on storage location.
   - Location or scope: `docs/DEVELOPMENT_PLAN_2026-05-03.md`, Phase 4; `src/NppAIAssistant.cpp`, `loadCopilotToken`; `src/shared/LLMApiClient.h`, `CopilotTokens`
   - Risk: Existing Copilot state keeps OAuth and access tokens in global process state, with no visible sign-out/delete path and no defined cleanup for device codes, debug strings, timers, failed refreshes, or token replacement.
   - Recommended action: Add a token lifecycle table: refresh/long-lived token in DPAPI only, short-lived access token in memory only, account metadata in SettingsStorage only. Add sign-out behavior that deletes DPAPI token, clears process token state, clears pending device code/debug/timer state, and updates UI. Do not treat source checks alone as OAuth validation.

2. Warning: Memory storage plan is underspecified for sensitive content.
   - Location or scope: `docs/DEVELOPMENT_PLAN_2026-05-03.md`, Phase 3; `src/shared/SettingsStorage.cpp`
   - Risk: SettingsStorage writes to the normal Notepad++ config INI path and is not protected by DPAPI. The plan says memory is user-authored context, but that context may still contain proprietary code snippets, local paths, customer text, or accidental secrets.
   - Recommended action: Make memory disabled by default, add per-entry enablement, add send-time disclosure, show exactly which entries are included, add delete/clear-all behavior, add a redaction/secret-pattern warning before save, and document roaming/backup exposure if SettingsStorage is used.

3. Warning: OAuth support could be read as provider login support even though real provider flows are not validated.
   - Location or scope: Phase 4; `src/NppAIAssistant.cpp` around `kEnabledProviders`, `loadCopilotToken`, and `invokeProvider`
   - Risk: The current code has Copilot device-flow code but excludes Copilot from enabled providers and returns a paused notice. The plan must not imply OpenAI, Gemini, Claude, or Copilot OAuth login is complete.
   - Recommended action: Rename the first OAuth slice to "OAuth storage/readiness model" and require a provider-specific gate before enabling sign-in UI. Store only opaque refresh/long-lived tokens in SecureStorage and short-lived access tokens in memory.

4. Warning: Supabase broker path needs a stricter non-goal statement.
   - Location or scope: Phase 4
   - Risk: Supabase-hosted-admin-oauth is a web-app/admin skill. This repo is a native desktop plugin. Applying it directly would produce false-positive hosted OAuth claims.
   - Recommended action: Mark Supabase broker work out of scope for this batch unless a separate hosted backend plan exists. No Supabase service-role key, OAuth client secret, hosted env file, or Supabase CLI-derived secret belongs in the plugin repo or plugin storage.

5. Warning: DPAPI protection needs its threat boundary stated.
   - Location or scope: `src/shared/SecureStorage.cpp`; `docs/SECURITY_REMEDIATION.md`
   - Risk: DPAPI + LocalAppData reduces offline and roaming exposure but does not protect against same-user malware or compromised Notepad++ plugins.
   - Recommended action: Add this limitation to OAuth and memory documentation, especially because refresh tokens can silently renew access.

## Review 2 - Notepad++ Plugin, Win32, and Release Workflow

### Findings

1. Critical: Async phase is under-specified for Win32/plugin lifetime safety.
   - Location or scope: `src/NppAIAssistant.cpp`, `sendPrompt`, `runSelectionCommand`, `invokeProvider`
   - Risk: A naive worker-thread change can touch HWND, Notepad++, Scintilla, or global state from a worker thread, race selection replacement, or write back after document changes.
   - Recommended action: Add an async design gate: capture immutable request snapshot on UI thread, run only network work on worker, return through `PostMessage`, mutate UI/Scintilla only on UI thread, revalidate HWND/plugin unload/buffer/selection before replacement, and define cancellation/unload behavior.

2. Warning: Settings dialog capacity is already near its practical limit.
   - Location or scope: `src/NppAIAssistantResources.rc`, `IDD_AIASSISTANT_SETTINGS DIALOGEX 0, 0, 380, 590`
   - Risk: Adding identity editor, rules editor, memory editor, token summary, OAuth status, and context templates into the same fixed dialog will cause clipping and poor localization behavior.
   - Recommended action: Add a tab control or separate secondary dialogs before expanding settings. Reserve feature ID ranges, tab order, DPI behavior, and screenshot gates at 100%, 125%, and 150%.

3. Warning: Waiting animation depends on async provider invocation, not just a timer.
   - Location or scope: `src/NppAIAssistant.cpp`, `sendPrompt`, `runSelectionCommand`, `invokeProvider`
   - Risk: Current calls are synchronous and block the UI thread. A timer animation added around the same synchronous call will not paint while blocked.
   - Recommended action: Move provider calls to a worker thread and marshal results back to the UI thread before claiming waiting animation support.

4. Warning: Release gate must clarify shipped architectures.
   - Location or scope: `NppAIAssistant.vcxproj`; release section of the plan
   - Risk: The project supports Win32, x64, and ARM64 configurations, but the plan only names x64 Release. Plugins Admin release metadata must match each intended architecture.
   - Recommended action: Treat x64 as the first validation target, then explicitly decide whether Win32 and ARM64 are in or out of the release batch before packaging.

5. Warning: Plugins Admin URL/version/list validation is incomplete.
   - Location or scope: `scripts/package-npp-ai-plugin.ps1`; `plugin-admin-metadata.json`; `docs/PLUGIN_ADMIN_SUBMISSION.md`
   - Risk: The metadata repository field is currently the GitHub repo homepage. Plugins Admin requires a direct downloadable `.zip` URL and SHA-256 of that exact final asset.
   - Recommended action: Package locally, upload final ZIP to GitHub Release, rerun package with direct `.zip` asset URL, assert `.npp-plugin-entry.json` repository ends in `.zip`, assert SHA-256 and DLL version match.

6. Suggestion: Context menu template IDs must avoid collision with existing static IDs.
   - Location or scope: `src/NppAIAssistant.cpp`, `kAiContextExplain` through `kAiContextFix`
   - Risk: Adding dynamic context menu commands with overlapping IDs can route to the wrong selection action.
   - Recommended action: Reserve a range, for example `kAiContextCustomTemplateBase = 100`, and map custom template slots by offset.

7. Suggestion: Version, ABI export, and package-install smoke gates should be explicit release tasks.
   - Location or scope: `src/NppAIAssistantVersion.h`, `plugin-admin-metadata.json`
   - Risk: Bumping version before a package is validated creates release drift; build-output install smoke can miss ZIP layout issues; ABI exports can regress silently.
   - Recommended action: Add a release-candidate checkpoint before version bump, verify DLL `FileVersion`/`ProductVersion`, check `dumpbin /exports`, and smoke test from extracted ZIP layout.

## Review 3 - UI, Prompt Architecture, Token Counting, and SidePilot Adaptation

### Findings

1. Warning: Plan overstates "system" prompt semantics for current provider calls.
   - Location or scope: `src/shared/LLMApiClient.cpp`; `docs/DEVELOPMENT_PLAN_2026-05-03.md`, Phase 1
   - Risk: Providers currently receive the entire assembled prompt as user content. A user-editable "System Identity" or Rules block can look stronger than mandatory single-turn/no-hidden-memory guardrails.
   - Recommended action: Call these prompt sections unless provider-native system/developer roles are implemented. Define a non-editable Mandatory System/Transparency block that editable identity/rules cannot remove.

2. Warning: "Module chip" adaptation needs a native UI definition.
   - Location or scope: Phase 1; SidePilot `identity-chip` is HTML/CSS, NppAIAssistant is native Win32 resources
   - Risk: The plan names module chips but does not define whether they are push buttons, a combo box plus Insert button, or another native control.
   - Recommended action: Define v1 as a compact token combo plus `Insert` button next to the identity/rules editor. This is easier to localize and avoids pretending Win32 static controls are web chips.

3. Warning: Prompt block source checks cannot verify real behavior alone.
   - Location or scope: Phase 1 and Phase 7
   - Risk: Script checks for block names can pass while block order, enable flags, or user request placement is wrong.
   - Recommended action: Add a canonical Npp order, a golden prompt fixture or scriptable prompt-builder verification path, and require preview and send path to share the same assembled string.

4. Warning: Token counter must be labeled as approximate and provider-independent.
   - Location or scope: Phase 7
   - Risk: Users may treat estimates as exact billing or context-window limits. Different providers tokenize text differently.
   - Recommended action: Define a `PromptEstimate` contract with per-block `chars`, `estimatedTokens`, and `included`; test ASCII, CJK, mixed text, resolved tokens, and section headers.

5. Suggestion: Token variables should be resolved at preview/send time, not persisted as resolved values.
   - Location or scope: token template storage
   - Risk: provider, model, timestamp, encoding, and line ending values go stale if resolved values are saved.
   - Recommended action: Persist raw template text and resolve only for preview and prompt assembly. Warn on unknown tokens.

6. Suggestion: Inline info panel should be an AI panel status strip first.
   - Location or scope: Phase 8
   - Risk: A floating Scintilla overlay has DPI, focus, z-order, and lifecycle risks that can destabilize the plugin.
   - Recommended action: Confirm Phase 8 as a status strip in the existing docked panel for this batch; defer editor overlay.

7. Suggestion: Documentation must distinguish shipped features from planned features.
   - Location or scope: Phase 9; `README.md`, `README_zh-TW.md`, `docs/USAGE.md`
   - Risk: Updating documentation for OAuth/memory before implementation can overstate capabilities.
   - Recommended action: Document "implemented", "experimental", and "planned" states explicitly.

## Assumptions

- The first implementation slice should be reviewable before attempting true async provider calls or real OAuth sign-in.
- This native plugin should preserve current GPL-3.0 licensing and Notepad++ plugin ABI.
- The corrected plan and code batch must not treat source checks as a substitute for real auth validation, UI smoke, or package smoke.

## Disposition

fix-and-rerun

## Next Action

Revise the plan into a corrected implementation plan that narrows the first code batch to prompt blocks, native token insertion, rules/assignment templates, and estimated token display, while explicitly deferring real OAuth sign-in, memory entry CRUD, async waiting animation, and release packaging until their prerequisites are implemented.

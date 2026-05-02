# NppAIAssistant Development Plan - 2026-05-03

## Scope

This plan covers the requested optimization and feature batch for the existing
Notepad++ plugin project. It keeps the plugin local-first, preserves the current
single-turn transparency promise, and treats OAuth, memory, and release work as
separate gates because they have different risk levels.

Current repo state at planning time:

- Repo root: `Q:\Projects\NppAIAssistant`
- Branch: `main`
- Worktree: clean
- Remote state: ahead of `origin/main` by 3 local commits
- Plugin identity: `NppAIAssistant.dll`, folder name `NppAIAssistant`
- Primary implementation surface: `src/NppAIAssistant.cpp`
- Native UI resource surface: `src/NppAIAssistantResources.h` and `src/NppAIAssistantResources.rc`
- Plain settings store: `%AppData%\Notepad++\plugins\config\NppAIAssistant.ini`
- Secret store: `%LocalAppData%\Notepad++\AIAssistant`, DPAPI protected

## Reference Inputs

- `Q:\Projects\SidePilot\extension\sidepanel.js`
  - Used for the System Identity editor pattern: module chips insert tokens into a template, and tokens resolve when saving or previewing.
- `Q:\Projects\SidePilot\extension\js\rules-manager.js`
  - Used for rules/template storage ideas: built-in templates, default seeding, source metadata, versioning, and reset/apply behavior.
- `Q:\Projects\SidePilot\docs\SIDEPANEL_DESIGN.md`
  - Used for the compact operator-console design direction, adapted to native Win32 controls rather than web sidepanel CSS.
- `Q:\Projects\SidePilot\docs\FEATURES.md`
  - Used for the prompt injection layer order: identity, rules, memory, system instructions, user message.
- `C:\Users\miles\.codex\skills\supabase-hosted-admin-oauth\SKILL.md`
  - Used only as an OAuth readiness boundary. This native plugin does not have a hosted `/admin` route, so Supabase is an optional broker pattern, not a current validation claim.
- `C:\Users\miles\.codex\skills\notepad-plus-plus-plugin-workflow\SKILL.md`
  - Used for release gates: build, package, ZIP root DLL, SHA-256, and Plugins Admin separation.

## Product Direction

NppAIAssistant should remain an explicit editor-side assistant:

- visible prompt construction
- local settings and secret storage
- no hidden memory unless the user explicitly enables a scoped memory block
- fast Notepad++ selection workflows
- native UI density suitable for repeated coding tasks

Design register: product UI. The UI should feel like a compact native operator panel, not a landing page or chat product. Use familiar Win32 affordances, short labels, scan-friendly sections, and inline status over decorative layout.

## Delivery Phases

### Phase 0 - Planning, Audit, and Review Gate

Deliverables:

- This development plan.
- Three independent external-audit style reviews of the plan.
- A consolidated correction plan integrating accepted review findings.
- One subagent review of the corrected plan before implementation.

Done when:

- Every numbered requirement from the user prompt is mapped to a concrete implementation slice or explicitly deferred with rationale.
- Cross-project references are attributed.
- The final execution plan has review gates and release gates.

### Phase 1 - Prompt System, System Identity, Rules, Templates

Goal:

Bring SidePilot-like System Identity and Rules control into the existing prompt builder without breaking the current single-turn model.

Implementation sketch:

- Add a small prompt block model inside `src/NppAIAssistant.cpp`:
  - `PromptBlockType::Identity`
  - `PromptBlockType::Rules`
  - `PromptBlockType::Memory`
  - `PromptBlockType::System`
  - `PromptBlockType::Assignment`
  - `PromptBlockType::UserRequest`
- Replace the current monolithic `buildEffectivePromptForConfig` assembly with helper functions that produce labeled blocks and then join them in order.
- Add a System Identity template:
  - default content describes NppAIAssistant, native Notepad++ context, single-turn behavior, provider model, and storage boundaries.
  - stored as non-secret SettingsStorage text.
  - reset restores the built-in default.
- Add module chip equivalents for native UI:
  - `{{PLUGIN_NAME}}`
  - `{{NPP_CONTEXT}}`
  - `{{PROVIDER}}`
  - `{{MODEL}}`
  - `{{LANGUAGE}}`
  - `{{ENCODING}}`
  - `{{LINE_ENDING}}`
  - `{{TIMESTAMP}}`
- Add Rules template support:
  - built-in basic core template.
  - built-in assignment template.
  - user-custom rules text.
  - auto-save on OK.
  - reset button restores default rules/template content.

Storage:

- Store non-secret templates and enable flags in SettingsStorage.
- Include schema-version migration defaults.
- Do not store rules in SecureStorage because they are not secrets.

Verification:

- Add script-level source checks for prompt block names, template defaults, and storage key separation.
- Build x64 Release.
- Manual plugin smoke: open settings, insert each token, preview resolved output, reset, save, reopen.

### Phase 2 - Display Scale and Native UI Layout

Goal:

Make panel and settings readability adjustable without relying only on the current chat font buttons.

Implementation sketch:

- Add `displayScalePercent` preference with range 80-150 and default 100.
- Apply scale to chat font, input font, prompt preview font, and selected dense labels.
- Keep dialog dimensions stable enough for Notepad++ docking; avoid controls shifting unpredictably.
- Add UI copy in English and Traditional Chinese.

Verification:

- Source check for clamp behavior and stored key.
- Manual plugin smoke at 80, 100, 125, and 150 percent.
- Screenshot gate after build using `system-screenshot` on the visible Notepad++ plugin window.

### Phase 3 - Memory Storage

Goal:

Add explicit, bounded memory storage while preserving the "no hidden memory" promise.

Implementation sketch:

- Add opt-in memory blocks, disabled by default.
- Store memory entries as non-secret structured text in SettingsStorage initially.
- Limit entries to a small bounded set, for example 10 entries and 3600 total prompt characters.
- Add fields:
  - title
  - content
  - enabled
  - updatedAt
  - source
- Inject only enabled memory entries into the `[Memory]` prompt block.
- Show memory block preview and token estimate separately.

Storage decision:

- Use SettingsStorage for v1 because memory content is user-authored context, not credentials.
- Warn in UI/documentation that secrets should not be placed in memory.
- Revisit JSON file storage only if SettingsStorage size or editing ergonomics become limiting.

Verification:

- Source checks for disabled-by-default memory injection.
- Manual smoke: add memory, send prompt, verify prompt preview includes memory; disable memory, verify prompt preview excludes memory.

### Phase 4 - OAuth Login and AI Provider Auth Storage

Goal:

Evaluate and prepare OAuth-based AI service login without overclaiming support for providers that do not expose native desktop OAuth flows.

Current state:

- API-key providers: OpenAI, Gemini, Claude.
- Copilot OAuth device flow exists in code, but the provider is not enabled in `kEnabledProviders` and `invokeProvider` currently returns a paused notice for Copilot.

Storage recommendation:

- Keep API keys in DPAPI SecureStorage.
- For OAuth, store refresh or long-lived OAuth token only in DPAPI SecureStorage.
- Keep short-lived access tokens in process memory and wipe on sign-out or expiration.
- Store non-secret OAuth metadata in SettingsStorage:
  - provider id
  - account display label
  - token expiry timestamp
  - last auth status
- If Supabase is introduced as a hosted OAuth broker, the plugin must store only the user session token locally. Supabase service role keys and OAuth client secrets must remain server-side.

Implementation slice:

- First implement an `AuthMethod` status model and UI labels for API key versus OAuth readiness.
- Do not claim OpenAI/Gemini/Claude OAuth until real provider flows are documented and tested.
- Keep Copilot sign-in behind an explicit provider enablement gate and release note.

Verification:

- Source check that OAuth tokens remain in SecureStorage.
- Manual sign-in test only after the chosen provider flow is enabled.
- Report separately:
  - OAuth storage prepared
  - provider login UI prepared
  - real OAuth sign-in tested or not tested

### Phase 5 - Waiting Animation and Async Request Flow

Goal:

Avoid a frozen native panel while output is pending.

Implementation sketch:

- Move provider invocation off the UI thread.
- Add request state:
  - idle
  - sending
  - waiting
  - completed
  - failed
- Add a lightweight timer-driven status animation in the panel, for example `Waiting`, `Waiting.`, `Waiting..`, `Waiting...`.
- Disable Send while a request is active.
- Keep Clear and Settings available unless a request state would corrupt state.

Verification:

- Build.
- Manual smoke with slow or failing provider.
- Confirm Notepad++ remains responsive while waiting.
- Confirm result returns to the chat display on the UI thread.

### Phase 6 - Context Menu Custom Templates

Goal:

Let users define three reusable right-click templates for selected text workflows.

Implementation sketch:

- Add three template records:
  - name
  - prompt prefix/body
  - replace-selection flag
  - enabled flag
- Store in SettingsStorage.
- Add three context menu entries when enabled.
- Built-in defaults can mirror explain/refactor/fix, but user-defined names should display in the context menu.
- Keep template prompt assembly through the same prompt block pipeline.

Verification:

- Manual smoke:
  - define three templates
  - right-click selected text
  - run each template
  - verify prompt and replacement behavior
- Source check for exactly three template slots and no secret storage.

### Phase 7 - Token Counter and Prompt Block Visibility

Goal:

Show system and task prompt blocks separately, then show a total token estimate.

Implementation sketch:

- Reuse the new prompt block model.
- Show a prompt block summary in settings:
  - Identity token estimate
  - Rules token estimate
  - Memory token estimate
  - System token estimate
  - Assignment token estimate
  - User request token estimate
  - Total estimate
- Use a local estimator for v1. Label as "estimate" because provider tokenizers differ.
- Keep exact provider token accounting as a later enhancement unless a provider response supplies usage metadata.

Verification:

- Unit-like source script checks for every block type in the summary.
- Manual prompt preview confirms block totals update when toggles/templates change.

### Phase 8 - Inline Information Panel

Goal:

Evaluate and implement a small inline panel for fast AI interaction while writing.

Recommendation:

- Use the existing docked AI panel first, adding an "Info" strip that can show:
  - selected provider/model
  - prompt token estimate
  - current template or preset
  - last request state
  - short action result
- Defer a true floating Scintilla overlay until after native panel changes are stable. A floating overlay has higher risk because it must track editor focus, DPI, selection, and Notepad++ window lifetime.

Verification:

- Native UI smoke.
- Screenshot check for clipping/overlap.

### Phase 9 - Documentation and Release Readiness

Deliverables:

- Update `README.md` and `README_zh-TW.md` with new capabilities and storage boundaries.
- Update `docs/USAGE.md`.
- Update `docs/DEVELOPMENT_LOG.md`.
- Add or update AI-assisted development disclosure according to the project documentation rules.
- Keep the GPL-3.0 license statement intact; do not replace it with MIT boilerplate.

Release gates:

- `scripts/verify-security-regressions.ps1`
- x64 Release build through `scripts/invoke-msbuild.ps1`
- package ZIP through `scripts/package-npp-ai-plugin.ps1`
- confirm ZIP root contains `NppAIAssistant.dll`
- compute SHA-256 for final ZIP
- smoke test from package install path
- separate plugin repo release from any `nppPluginList` update

## External Audit Questions

Ask reviewers to evaluate:

1. Does the plan preserve the current single-turn transparency promise while adding explicit memory?
2. Is the OAuth storage model safe and honest about what has not been validated?
3. Are the prompt block and token counter designs testable without a full provider call?
4. Is the native Win32 UI scope realistic for the requested UIUX improvements?
5. Are release and Plugins Admin gates complete enough for a Notepad++ plugin?

## Initial Risk Register

| Risk | Impact | Mitigation |
|---|---|---|
| OAuth provider support differs by vendor | High | Separate storage readiness, UI readiness, and real sign-in validation |
| Memory feature conflicts with no-hidden-memory promise | High | Disabled by default, explicit preview, documentation warning |
| UI thread blocks during provider calls | High | Async request flow before animation polish |
| Token estimates are mistaken for exact billing counts | Medium | Label estimates clearly and keep provider usage as separate future work |
| Native settings dialog becomes overcrowded | Medium | Add tabs or grouped sections before adding more controls |
| Notepad++ ABI or package layout regression | High | Run plugin workflow gates before release |

## Proposed First Implementation Batch

After plan review, implement the smallest high-value slice:

1. Prompt block model and block-based preview.
2. System Identity template with module token resolution.
3. Rules/basic assignment templates with reset and SettingsStorage persistence.
4. Token estimate summary by block.
5. Documentation update for prompt blocks, explicit memory/OAuth status, and AI disclosure.

This first batch avoids UI-thread async and real OAuth changes until the core prompt system is reviewable.

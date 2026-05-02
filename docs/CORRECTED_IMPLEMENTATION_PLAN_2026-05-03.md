# Corrected Implementation Plan - 2026-05-03

This document folds the audit findings from
`docs/EXTERNAL_AUDIT_2026-05-03.md` into the executable plan.

## Accepted Corrections

1. OAuth is renamed to "OAuth storage and readiness" until one provider has a
   real tested sign-in path.
2. Supabase is treated only as a possible hosted broker. The native plugin must
   not contain Supabase service-role keys, OAuth client secrets, or hosted admin
   assumptions.
3. Memory v1 is disabled by default, visibly injected only when enabled, and
   documented as plain local config unless encrypted memory is separately
   requested.
4. The settings dialog needs a layout container before large new editor surfaces
   are added.
5. Waiting animation is blocked by synchronous provider calls. It cannot be
   claimed until provider calls run off the UI thread.
6. Native "module chips" will be implemented as a token combo plus an Insert
   button in v1.
7. Token counts are estimates. The UI and docs must not call them exact usage.
8. x64 is the first build target. Win32 and ARM64 must be explicitly included or
   excluded before a release package is called complete.
9. Current provider calls receive a single assembled prompt as user content. The
   implementation must call the pieces "prompt sections" unless provider-native
   system/developer roles are implemented.
10. A non-editable Mandatory System/Transparency section must always be present
    and must not be removable through editable templates.
11. Async waiting animation is blocked until a worker-thread design safely
    marshals results back to the UI thread.
12. Documentation and UI must distinguish these statuses:
    `storage model implemented`, `provider UI prepared`, and
    `real provider sign-in validated`.

## Implemented Code Batch

The implemented batch expanded after the first gate and now includes:

- Add a prompt section model.
- Assemble prompt sections in explicit order:
  1. Mandatory System
  2. Identity
  3. Rules
  4. System
  5. Assignment
  6. User Request
- Add built-in defaults:
  - Mandatory single-turn transparency system section
  - System Identity template
  - Basic Core Rules template
  - Assignment template
- Define `System` as non-editable generated current prompt-profile content:
  reply language, encoding preference, line ending, detail level, scenario
  modules, and output rules. It is a prompt section, not a provider-native
  system role.
- Add SettingsStorage persistence for:
  - identity enabled flag
  - identity template
  - rules enabled flag
  - rules template
  - assignment template enabled flag
  - assignment template
- Add token resolution for native system variables:
  - `{{PLUGIN_NAME}}`
  - `{{PROVIDER}}`
  - `{{MODEL}}`
  - `{{LANGUAGE}}`
  - `{{ENCODING}}`
  - `{{LINE_ENDING}}`
  - `{{TIMESTAMP}}`
- Persist raw template text. Resolve variables only during preview and prompt
  assembly. Unknown variables should remain visible and generate a warning in
  the preview text.
- Add an estimated-token summary to the prompt preview text using a
  `PromptEstimate` contract:
  - `label`
  - `chars`
  - `estimatedTokens`
  - `included`
- Add a secondary Prompt Sections dialog with enable toggles, token insertion,
  and reset buttons.
- Add explicit Memory storage:
  - disabled by default
  - bounded to 3,600 characters
  - stored as non-secret SettingsStorage text
  - injected only as a visible Memory section
- Add persistent Display Scale through the `A+` and `A-` panel controls.
- Move provider requests off the UI thread:
  - worker thread performs provider/network call
  - timer-driven waiting message updates the panel
  - result returns through `WM_AI_REQUEST_COMPLETE`
  - Scintilla replacement is performed only on the UI thread after buffer check
- Add three configurable right-click context templates with optional selection
  replacement.
- Update the security regression script to check:
  - prompt templates are saved through SettingsStorage
  - OAuth/Copilot token keys remain in SecureStorage
  - prompt section labels exist in the builder
  - token summary uses estimated wording
- Use one shared builder that returns ordered prompt sections and the final
  joined prompt text. Both preview and send must consume that same builder path.
- Add a scriptable prompt builder verification path if feasible. If not feasible
  in the first pass, manually compare the preview prompt body to the exact sent
  prompt for the same request and report the limitation.

## Explicit Deferrals

These are not complete after the first code batch:

- Real OpenAI/Gemini/Claude OAuth login.
- Enabling Copilot as a shipped provider.
- Full multi-entry Memory CRUD, search, relevance scoring, and encrypted memory.
- Native tabbed settings shell; current implementation uses secondary dialogs.
- Official release publication and Plugins Admin update.
- Direct `.zip` GitHub Release URL for the generated Plugins Admin entry.
- Final visual screenshot gate.

## UI Compromise for Batch 1

The current settings dialog is a fixed `380 x 590` Win32 resource and is already
dense. Batch 1 may use the existing prompt preview area to show the assembled
section preview and estimated-token summary. Large editors, token insertion
controls, and reset buttons should wait for a tabbed or secondary settings
surface unless a small, non-clipping layout change is proven by build and
screenshot.

When the editor UI is added, native module chips will be a token combo plus an
`Insert` button. Web-style chip styling from SidePilot is not a native Win32 v1
requirement.

## OAuth Storage Lifecycle Gate

OAuth implementation is not part of Batch 1, but future OAuth code must follow
this lifecycle:

| Data | Storage | Cleanup |
|---|---|---|
| Refresh or long-lived OAuth token | `SecureStorage` only | Delete on sign-out, failed revocation, or account switch |
| Short-lived access token | memory only | Wipe on expiry, refresh failure, sign-out, plugin unload |
| Device code and polling metadata | memory only | Clear when complete, failed, cancelled, or timed out |
| Account label, expiry timestamp, auth status | `SettingsStorage` | Clear on sign-out or account switch |
| Provider client secret or Supabase service-role key | never in plugin | server-side only if a separate hosted broker exists |

Validation language must report these separately:

- storage model implemented
- provider UI prepared
- real provider sign-in validated

Default status is `not validated` until a real provider flow succeeds.

## Memory Storage Gate

Implemented v1 satisfies:

- master toggle off by default
- exact prompt preview inclusion as a visible Memory section
- no relevance auto-selection
- clear action in Memory dialog
- warning that v1 plaintext memory is stored in normal plugin settings
- bounded content length

Future full Memory CRUD needs either a length-bounded serialization format with
parser tests or a separate UTF-8 JSON file with backup-on-write.

## Async Waiting Animation Gate

Implemented v1 follows:

- capture immutable request snapshot on the UI thread
- run only network/provider work on a worker thread
- marshal completion to the UI thread with `PostMessage` or equivalent
- mutate `g_chatHistory`, controls, Notepad++ handles, and Scintilla only on the UI thread
- revalidate panel window, plugin unload state, current buffer id, and selection range before replacement
- wipe request-local API keys/tokens on every success and failure path
- smoke test slow provider, failed provider, closing panel during request, and switching documents before replacement remains a manual native-app gate

## Done Criteria for Batch 1

- x64 Release build succeeds.
- `scripts/verify-security-regressions.ps1` passes.
- The prompt preview shows section labels and estimated token counts.
- The default effective prompt includes Mandatory System, Identity, Rules,
  System, Assignment, and User Request sections in the expected order.
- The `System` section is non-editable generated prompt-profile content, not a
  provider-native role claim.
- Preview and send use the same shared prompt-section builder.
- Editable templates cannot remove the Mandatory System section.
- API keys and OAuth token storage remain in SecureStorage.
- Non-secret prompt templates are in SettingsStorage.
- Documentation states that real OAuth login is not yet shipped.
- Local package smoke succeeds, but Plugins Admin entry is not publishable until
  a direct GitHub Release `.zip` URL is supplied.

## Next Batch After Batch 1

1. Add real provider-specific OAuth sign-in only after a provider flow is chosen.
2. Add full multi-entry Memory CRUD/search only if the single memory block proves insufficient.
3. Add a tabbed settings shell to reduce fixed-dialog density.
4. Supply a direct GitHub Release `.zip` URL and rerun Plugins Admin packaging.
5. Run native Notepad++ visual screenshot gate.

## Release Gate Corrections

Release work is a separate batch. Before publishing:

- Decide x64-only versus Win32/x64/ARM64 matrix.
- Update `src/NppAIAssistantVersion.h` only at release-candidate checkpoint.
- Build every intended architecture.
- Package every intended architecture.
- Upload final ZIP assets before generating final Plugins Admin entries.
- Ensure each Plugins Admin `repository` is a direct downloadable `.zip` URL.
- Ensure each `id` is the SHA-256 of the final downloadable ZIP.
- Verify DLL `FileVersion` and `ProductVersion` match the entry version.
- Verify ABI exports with `dumpbin /exports` or equivalent:
  `setInfo`, `getName`, `getFuncsArray`, `beNotified`, `messageProc`, `isUnicode`.
- Smoke test from extracted ZIP layout, not only build output.

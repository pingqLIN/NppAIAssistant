# Security Remediation Backlog

Last reviewed: 2026-03-12

This document turns the current API key storage review into an implementation-oriented remediation backlog for `NppAIAssistant`.

## Status

Implemented on 2026-03-12:

- `SEC-01` Remove API keys from URLs and centralize secret redaction
- `SEC-02` Move secret storage out of Roaming AppData and harden the storage directory ACL
- `SEC-03` Store only secrets in secure storage
- `SEC-06` Keep symbol files and local build artifacts out of redistribution paths

Implemented on 2026-04-26:

- `SEC-04` Reduce in-memory secret lifetime further
- partial `SEC-05` Add repeatable regression coverage for secret-loading and redaction invariants

Still open:

- `SEC-05` Add broader security regression coverage
- `SEC-07` Evaluate a future abstraction for OS-managed secret storage

## Scope

Reviewed files:

- `src/shared/SecureStorage.cpp`
- `src/shared/SecureStorage.h`
- `src/shared/LLMApiClient.cpp`
- `src/NppAIAssistant.cpp`
- `scripts/package-npp-ai-plugin.ps1`

Observed current behavior:

- Secrets and many non-secret preferences are stored as separate `.key` files under `%AppData%\Notepad++\AIAssistant`.
- Local storage uses Windows DPAPI via `CryptProtectData` and `CryptUnprotectData`.
- Gemini requests currently place the API key in the request URL query string.
- OpenAI and Claude use request headers for authentication.
- The packaged release zip currently includes the DLL and docs only; it does not include the PDB.

## Summary

The current implementation is better than plain text storage, but its protection boundary is the current Windows user account. That means:

- Offline file inspection is mitigated reasonably well.
- Other processes running as the same user can still read the encrypted blobs and ask DPAPI to decrypt them.
- The Gemini integration currently expands the secret exposure surface by placing the API key in the URL.

## Priority Order

## P0

### SEC-01: Remove API keys from URLs and centralize secret redaction

Current state:

- `LLMApiClient::listGeminiModels` and `LLMApiClient::callGemini` build URLs with `?key=<apiKey>`.
- `HttpClient` includes the full URL in parse errors.
- Query-string secrets are more likely to leak into logs, proxy traces, screenshots, crash reports, and copied error messages.

Recommended change:

- Prefer a provider-supported authentication mechanism that does not place the Gemini API key in the URL.
- Add a single redaction path in `HttpClient` so any future error message strips secrets from URLs and headers before surfacing them to the UI.
- Treat provider auth material as sensitive regardless of whether it came from a URL, header, or response body.

Implementation notes:

- Update the Gemini request builder in `src/shared/LLMApiClient.cpp`.
- Add URL and header redaction helpers near the existing auth redaction logic.
- Make sure `HttpResponse.errorMessage` never contains raw secrets.

Acceptance criteria:

- No user-visible error string contains a full API key.
- No code path concatenates a secret into a URL string without a matching redaction guard.
- Manual failure tests for Gemini do not expose the key in dialogs or debug messages.

### SEC-02: Move secret storage out of Roaming AppData and harden the storage directory ACL

Current state:

- `SecureStorage::getStoragePath()` writes to `%AppData%\Notepad++\AIAssistant`.
- The directory is created with inherited permissions only.
- Roaming profile locations increase the number of systems and backups that may receive encrypted blobs.

Recommended change:

- Store secrets under `%LocalAppData%` instead of `%AppData%`.
- Apply an explicit restrictive DACL when creating the secret storage directory so access is intentionally limited to the current user, `SYSTEM`, and administrators.
- Keep non-secret settings elsewhere if they still need roaming behavior.

Implementation notes:

- Update `src/shared/SecureStorage.cpp` to use a local-only path for secrets.
- Add one-time migration logic from the existing roaming folder to the new location.
- Migration should preserve current users without forcing them to re-enter keys.

Acceptance criteria:

- New installs write secrets only under `%LocalAppData%`.
- Existing installs automatically migrate and continue working.
- The secret directory ACL is explicitly set, not just inherited.

### SEC-03: Store only secrets in secure storage

Current state:

- `SecureStorage` is used for API keys, Copilot tokens, and non-secret UI preferences such as provider selection and prompt settings.
- This mixes confidentiality data with ordinary configuration and makes future migration harder.

Recommended change:

- Reserve `SecureStorage` for credentials and tokens only.
- Move non-secret preferences to a conventional config file format such as JSON, XML, or INI under the normal plugin config path.
- Keep one clear boundary: secrets in secure storage, preferences in normal config.

Implementation notes:

- Split the current `loadConfig` and `saveConfig` logic in `src/NppAIAssistant.cpp`.
- Secret keys should remain in the secure store.
- Preferences should be serialized separately with an explicit schema version.

Acceptance criteria:

- Only credential material is stored in the secure storage directory.
- Provider choice, prompt options, UI language, and similar preferences are no longer stored as `.key` blobs.
- Existing settings are migrated without silent loss.

## P1

### SEC-04: Reduce in-memory secret lifetime

Current state:

- Provider API keys are loaded on demand from `SecureStorage` instead of being hydrated into `g_config` during startup.
- The settings dialog still holds edited provider keys in memory while the dialog is open, which is expected for an editable UI surface.
- Copilot auth tokens remain in long-lived process state while the user is signed in because the current flow needs them for polling and token refresh.

Recommended change:

- Keep decrypted secrets in memory for the shortest practical duration.
- Avoid storing provider keys in long-lived global state when a shorter-lived fetch-on-demand pattern is feasible.
- Clear temporary buffers after request construction where practical.

Implementation notes:

- Review `src/NppAIAssistant.cpp` and `src/shared/LLMApiClient.cpp`.
- Avoid over-promising perfect memory secrecy on Windows and C++; the goal is exposure reduction, not absolute prevention.
- If a full redesign is too invasive, start by reducing duplicate copies and explicitly clearing temporary buffers after use.

Acceptance criteria:

- The code no longer keeps more copies of the same secret than necessary.
- Temporary auth strings are cleared after request submission where practical.
- The design notes document any remaining long-lived secret state and why it still exists.

### SEC-05: Add security regression tests and a manual verification checklist

Current state:

- The repo now has a manual checklist plus a lightweight scripted regression pass for key-loading and redaction invariants.
- There is still no compiled automated test target for helper-level secret handling.

Recommended change:

- Add targeted tests or scripted checks for secure-storage round-trip, migration, empty-value deletion, and redaction behavior.
- Add a release checklist item covering secret handling.

Implementation notes:

- At minimum, add a manual checklist under `docs/`.
- Prefer a small automated test surface for helper functions that do not require live provider calls.

Acceptance criteria:

- There is a repeatable verification flow for storage migration and redaction.
- A failed Gemini or HTTP parse path is explicitly tested for key leakage.
- Release preparation includes a secret-handling check.

## P2

### SEC-06: Keep symbol files and local build artifacts out of redistribution paths

Current state:

- The packaged zip produced by `scripts/package-npp-ai-plugin.ps1` does not include the PDB, which is good.
- However, local build output folders can still contain `.pdb` files next to the DLL, and ad hoc redistribution from build folders remains possible.

Recommended change:

- Keep the packaging script as the only supported redistribution path.
- Add a note in release documentation to avoid sharing raw build output folders.
- Optionally add a packaging validation step that fails if symbol files are staged for release.

Acceptance criteria:

- Official release assets never include `.pdb` files.
- Release documentation names the packaging script as the supported distribution method.

### SEC-07: Evaluate a future abstraction for OS-managed secret storage

Current state:

- DPAPI file blobs are simple and acceptable for an initial implementation.
- The design is tightly coupled to one storage mechanism and one directory structure.

Recommended change:

- Consider a future secret-storage abstraction so the plugin can evolve toward a Windows-managed credential store or other OS-integrated option without another large config migration.

Acceptance criteria:

- Not required for the next release.
- Keep this as a follow-up design item after the P0 and P1 work is complete.

## Suggested Delivery Plan

1. Implement `SEC-01` first because it directly reduces accidental key disclosure.
2. Implement `SEC-02` and `SEC-03` together because both require storage migration.
3. Implement `SEC-04` after the storage boundary is simplified.
4. Finish with `SEC-05` so the new behavior stays protected.

## Recommended Issue Breakdown

1. `P0` Harden Gemini auth handling and redact secrets from all error paths
2. `P0` Move secret storage to LocalAppData with explicit ACL and migration
3. `P0` Split secrets from ordinary settings storage
4. `P1` Reduce long-lived in-memory secret exposure
5. `P1` Add secret-handling verification coverage
6. `P2` Enforce release packaging hygiene for symbol files

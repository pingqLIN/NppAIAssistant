# Development Log

## 2026-04-26

### Secret lifetime reduction

- Stopped loading OpenAI, Gemini, and Claude API keys into the long-lived global `g_config` state during plugin startup.
- Switched provider requests to load secrets from `SecureStorage` on demand and wipe the transient request-local copy after use.
- Limited settings-dialog secret lifetime to the dialog editing flow instead of the full plugin session.

### Repeatable verification and loop state

- Added `scripts/verify-security-regressions.ps1` for a small repeatable regression check over the secret-loading and redaction invariants.
- Added `scripts/project-development-loop-state.ps1` so time-boxed development runs can persist the active batch, deadline, checkpoint, and next action outside the live agent session.
- Added `scripts/project-development-loop-overnight.ps1` for overnight heartbeat logging and resumable supervisor state.
- Added `scripts/doctor-msbuild-filetracker.ps1` to capture reproducible MSBuild/FileTracker diagnostics into repo-local logs.

## 2026-03-12

### Security storage refactor

- Moved secret storage from roaming AppData to `%LocalAppData%\Notepad++\AIAssistant`.
- Kept API keys and OAuth tokens in DPAPI-protected storage.
- Added lazy migration for legacy secret blobs from the old roaming path.
- Applied an explicit ACL to the local secret directory for the current user, `SYSTEM`, and administrators.

### Settings storage split

- Added `SettingsStorage` for non-secret preferences.
- Moved provider selection, prompt options, UI language, and similar settings into `%AppData%\Notepad++\plugins\config\NppAIAssistant.ini`.
- Added first-run migration from legacy secure preference blobs into the new plain settings file.
- Added cleanup so legacy preference `.key` files are removed after migration or save.

### Network secret handling

- Updated Gemini model listing and generation requests to send the API key in the `x-goog-api-key` header instead of the URL query string.
- Added URL redaction in `HttpClient` so parse errors do not echo raw query-string secrets.
- Reduced temporary in-memory exposure by wiping request-local provider keys after use where practical.

### Packaging and documentation

- Added security remediation and verification documents to the project docs set.
- Updated README and usage documentation to describe the new secret and settings storage layout.
- Hardened the packaging script with a symbol-file guard so `.pdb` files are not staged for release.

### Verification

- Release x64 build completed successfully with MSBuild on 2026-03-12.
- Packaging validation will now fail if a staged `.pdb` file is detected.

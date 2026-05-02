# Development Log

## 2026-05-03

### Prompt section groundwork

- Added a reviewed development plan, external audit report, and corrected
  implementation plan for the requested prompt, OAuth, UI, memory, and release
  optimization batch.
- Reworked prompt assembly into visible sections: Mandatory System, Identity,
  Rules, System, Assignment, and User Request.
- Added built-in identity, basic rules, and assignment templates with
  SettingsStorage persistence for non-secret template text.
- Added template token resolution for plugin/provider/model/language/encoding,
  line ending, and timestamp values at preview/send time.
- Added estimated token summary output to the prompt preview.
- Added a secondary Prompt Sections dialog for editing Identity, Rules, and
  Assignment templates with token insertion, enable toggles, and reset buttons.
- Persisted AI panel display scale through the existing `A+` and `A-` toolbar
  controls, clamped from 80% to 150%.
- Moved provider requests off the UI thread and added a timer-driven waiting
  message while responses are pending.
- Added explicit Memory storage with a visible prompt section, disabled by
  default, bounded to 3,600 characters, and stored as non-secret settings.
- Added three configurable right-click context templates with optional
  selection replacement.
- Documented that real OAuth login remains planned rather than shipped in this
  batch.
- Added package readiness metadata so local package smoke cannot be mistaken for
  a publishable Plugins Admin entry without a direct HTTPS `.zip` release URL.
- Added `scripts/smoke-package-install.ps1` to verify the packaged ZIP can be
  staged into a temporary Notepad++ plugin layout without PDB files.
- Added `docs/COMPLETION_AUDIT_2026-05-03.md` to map the full development-loop
  objective to concrete evidence and remaining gates.

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
- Added `scripts/invoke-msbuild.ps1` to normalize missing Windows environment variables such as `SystemDrive`, `ProgramData`, and `LOCALAPPDATA` before invoking MSBuild.

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

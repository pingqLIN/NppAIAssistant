# Security Verification Checklist

Use this checklist when validating secret handling changes or preparing a release.

## Scripted regression pass

1. Run `scripts/verify-security-regressions.ps1`.
2. Confirm the script reports only `PASS` lines.
3. If an overnight run is being prepared, initialize `scripts/project-development-loop-overnight.ps1 -NoLoop` so the durable state file already captures the active batch and telemetry status.

## Storage split

1. Launch the plugin with no prior settings and save a provider API key plus a few prompt preferences.
2. Confirm secret files are created under `%LocalAppData%\Notepad++\AIAssistant`.
3. Confirm ordinary preferences are written to `%AppData%\Notepad++\plugins\config\NppAIAssistant.ini`.
4. Confirm non-secret values are no longer written as `.key` files.

## Migration

1. Start from a profile that still has legacy secure `.key` files in `%AppData%\Notepad++\AIAssistant`.
2. Launch the updated plugin.
3. Confirm provider keys still work without manual re-entry.
4. Confirm ordinary preference blobs are removed from secure storage after migration.
5. Confirm new secret blobs exist in `%LocalAppData%`.

## Secret redaction

1. Trigger a failing Gemini request with an invalid key or forced network failure.
2. Confirm the UI error message does not include the raw API key.
3. Trigger a URL parse failure in `HttpClient` with a test URL containing `key=...`.
4. Confirm the reported URL shows `<redacted>` instead of the secret value.

## Release packaging

1. Run `scripts/package-npp-ai-plugin.ps1`.
2. Confirm the script succeeds with the expected zip output.
3. Confirm no `.pdb` files are present in `dist/_stage` or the final zip.
4. Confirm packaged docs include the updated README and security docs.
5. Run `scripts/smoke-package-install.ps1`.
6. Confirm `RootDllPresent`, `RequiredDocsPresent`, and `PdbPresent: False`.
7. Run `scripts/verify-release-readiness.ps1 -Platform x64 -Configuration Release`.
8. Confirm that the release readiness gate validates the manifest, package hash,
   DLL version, ZIP layout, plugin-list fields, and Notepad++ export names.
9. Treat `PluginsAdminReady: False` as expected for local package smoke until a
   final HTTPS `.zip` release URL is supplied.
10. After regenerating with a final direct HTTPS `.zip` URL, run
    `scripts/verify-release-readiness.ps1 -Platform x64 -Configuration Release -RequirePluginsAdminReady`.

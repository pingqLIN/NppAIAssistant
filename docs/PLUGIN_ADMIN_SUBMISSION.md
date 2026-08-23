# Plugins Admin Submission Guide

This document tracks what is needed to submit `NppAIAssistant` to the official Notepad++ Plugins Admin list.

## Official Requirements

Based on the Notepad++ User Manual and the official `nppPluginList` schema, the important rules are:

1. Submit the correct architecture JSON file:
   - `pl.x86.json` for 32-bit
   - `pl.x64.json` for 64-bit
   - `pl.arm64.json` for ARM64
2. `folder-name` must be unique.
3. The plugin DLL name must match the `folder-name`.
4. `id` must be the SHA-256 hash of the downloadable zip package.
5. `version` must exactly match the plugin DLL binary version.
6. `repository` must be a direct downloadable `.zip` URL.
7. Only zip packaging is supported.
8. The plugin DLL must be placed at the root level of the zip file.
9. Optional extra files can also be included.
10. If a `doc/` folder exists in the zip, the contents will be installed into `plugins/doc/<folder-name>/`.
11. The current schema also requires:
   - `description`
   - `author`
   - `homepage`

## Recommended Tag and Version

Recommended release setup for this project:

- GitHub release tag: `v0.1.0`
- Plugin DLL version: `0.1.0.0`
- Release asset file name: `NppAIAssistant-0.1.0.0-x64.zip`

This keeps the public Git tag clean while preserving the exact four-part DLL version required for plugin submission.

## Current Status for This Repo

### Already aligned
- Plugin name and DLL name both use `NppAIAssistant`
- A standalone plugin repository exists with root-level build files
- A release DLL is produced successfully for `x64`
- DLL version resource is embedded in the binary
- Packaging script builds a Plugins Admin style zip
- Packaging script now emits a schema-shaped JSON entry with all required metadata fields
- The package places `NppAIAssistant.dll` at the zip root
- The packaging script rejects staged `.pdb` symbol files before zip creation
- The release readiness script validates the manifest, ZIP hash, DLL version,
  ZIP layout, plugin-list fields, and required Notepad++ exports
- Documentation is placed under `doc/NppAIAssistant/`
- The package smoke script can stage the zip into a temporary Notepad++ plugin
  layout without touching the installed Notepad++ directory

### Remaining external step
- Publish the generated zip as a GitHub Release asset
- Re-run the packaging script with `-ReleaseUrl` pointing directly to that
  HTTPS `.zip` asset
- Submit a PR to `https://github.com/notepad-plus-plus/nppPluginList`
- Optionally add `x86` or `arm64` builds if you want those architectures listed

## How to Produce a Submission Package

Build the plugin first:

```powershell
.\scripts\invoke-msbuild.ps1 -Configuration Release -Platform x64
```

Create the package:

```powershell
.\scripts\package-npp-ai-plugin.ps1 -Platform x64
```

This will generate:
- `dist/NppAIAssistant-0.1.0.0-x64.zip`
- `dist/NppAIAssistant-0.1.0.0-x64.plugin-admin.json`
- `dist/NppAIAssistant-0.1.0.0-x64.npp-plugin-entry.json`

The script reads the DLL version directly and will fail if you try to package with a mismatched version string.
Without `-ReleaseUrl`, the manifest is marked `PluginsAdminReady: false` and is
valid for local package smoke only.

Smoke test the generated zip layout:

```powershell
.\scripts\smoke-package-install.ps1
```

Run the release readiness gate:

```powershell
.\scripts\verify-release-readiness.ps1 -Platform x64 -Configuration Release
```

This passes for a local package while `PluginsAdminReady` is false. Use
`-RequirePluginsAdminReady` only after publishing a final direct HTTPS ZIP
asset and regenerating the package with that exact URL.

## Recommended Submission Workflow

1. Build the release DLL
2. Run the packaging script
3. Create a GitHub Release and upload the generated zip
4. Re-run the packaging script with the final release URL:

```powershell
.\scripts\package-npp-ai-plugin.ps1 -Platform x64 -ReleaseUrl "https://github.com/pingqLIN/NppAIAssistant/releases/download/v0.1.0/NppAIAssistant-0.1.0.0-x64.zip"
```

5. Verify the final package:

```powershell
.\scripts\verify-release-readiness.ps1 -Platform x64 -Configuration Release -RequirePluginsAdminReady
```

6. Copy the generated entry JSON into the correct `nppPluginList` architecture file
7. Test locally if needed with the official Plugins Admin local-test flow
8. Submit the PR

## Metadata Source

Plugin submission metadata is stored in:

`plugin-admin-metadata.json`

Update this file before packaging if you need to change:
- description
- author
- homepage
- repository homepage

## Notes

- The current repo is ready for `x64` submission preparation.
- It is not yet ready for `x86` or `arm64` distribution unless those builds are added and tested.
- The release asset URL is not yet present in the current local manifest. Do not
  submit to `nppPluginList` until the GitHub Release zip URL is supplied and the
  package is regenerated with `PluginsAdminReady: true`.

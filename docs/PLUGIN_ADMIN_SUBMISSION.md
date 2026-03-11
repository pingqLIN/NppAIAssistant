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
- Documentation is placed under `doc/NppAIAssistant/`

### Still required at release time
- Publish a GitHub Release with the generated `.zip`
- Use the final release asset URL as the `repository` field
- Confirm the published zip hash is the same as the generated `id`
- Submit a PR to `https://github.com/notepad-plus-plus/nppPluginList`
- Optionally add `x86` or `arm64` builds if you want those architectures listed

## How to Produce a Submission Package

Build the plugin first:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" "NppAIAssistant.vcxproj" /p:Configuration=Release /p:Platform=x64 /m
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

## Recommended Submission Workflow

1. Build the release DLL
2. Run the packaging script
3. Create a GitHub Release and upload the generated zip
4. Re-run the packaging script with the final release URL:

```powershell
.\scripts\package-npp-ai-plugin.ps1 -Platform x64 -ReleaseUrl "https://github.com/pingqLIN/NppAIAssistant/releases/download/v0.1.0/NppAIAssistant-0.1.0.0-x64.zip"
```

5. Copy the generated entry JSON into the correct `nppPluginList` architecture file
6. Test locally if needed with the official Plugins Admin local-test flow
7. Submit the PR

## Metadata Source

Plugin submission metadata is stored in:

`plugin-admin-metadata.json`

Update this file before packaging if you need to change:
- description
- author
- homepage
- repository placeholder

## Notes

- The current repo is ready for `x64` submission preparation.
- It is not yet ready for `x86` or `arm64` distribution unless those builds are added and tested.
- Before the actual PR, replace placeholder GitHub URLs with the final public repository and release asset URLs.

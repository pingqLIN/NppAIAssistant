# Plugins Admin Submission Guide

This document describes the project-specific Notepad++ Plugins Admin update flow.
The authoritative release gates are in `docs/RELEASE_WORKFLOW.md`; the active
version state is tracked in `docs/releases/v0.2.0.0-release-plan.md`.

Rules last re-checked against the official Notepad++ documentation, schema and
validator on 2026-08-23.

## Current Project Status

`NppAIAssistant` already exists in the official x64 Plugins Admin list at
version `0.1.0.0`. The next submission must therefore **update the existing x64
entry**. It must not add a duplicate entry.

Target candidate for the current release plan:

- DLL version: `0.2.0.0`
- Git tag: `v0.2.0.0`
- x64 release asset: `NppAIAssistant-0.2.0.0-x64.zip`

No official update PR should be opened until all release gates pass and the user
explicitly approves submission.

## Official Requirements

Current Notepad++ rules require:

1. Update the correct architecture file (`src/pl.x64.json` for x64).
2. `folder-name` must remain unique.
3. The DLL basename must match `folder-name`; for this project both are
   `NppAIAssistant`.
4. `id` is the SHA-256 of the downloadable ZIP bytes.
5. `version` must exactly match the plugin DLL binary version.
6. `repository` must be a direct downloadable ZIP URI.
7. The package must be ZIP format.
8. `NppAIAssistant.dll` must be at the ZIP root.
9. Optional files may be included; a `doc/` tree is installed under the plugin
   documentation location.
10. The current schema requires `folder-name`, `display-name`, `version`, `id`,
    `repository`, `description`, `author`, and `homepage`.

Official references:

- https://github.com/notepad-plus-plus/nppPluginList
- https://github.com/notepad-plus-plus/nppPluginList/blob/master/pl.schema
- https://github.com/notepad-plus-plus/nppPluginList/blob/master/validator.py
- https://npp-user-manual.org/docs/plugins/#plugins-admin

The official validator performs more than schema validation: it downloads each
plugin archive, checks the SHA-256, verifies ZIP validity and the expected DLL,
reads the DLL binary version, and checks uniqueness constraints.

## Canonical Package Rule

A release version must have exactly one canonical x64 ZIP byte sequence.

Do not use this unsafe sequence:

1. create ZIP A locally;
2. upload ZIP A;
3. re-run packaging and create ZIP B;
4. use ZIP B's hash in Plugins Admin.

Even if ZIP A and ZIP B contain the same files, recompression can produce a
different SHA-256.

Use this sequence instead:

1. Build the reviewed release commit.
2. Create the canonical ZIP once.
3. Verify and record its SHA-256.
4. Upload **that exact file** as the GitHub Release asset.
5. Download the published asset back from its final URL.
6. Calculate SHA-256 again.
7. Require the pre-upload hash, post-download hash and `id` to be identical.

If GitHub Actions publishes the release, the release job must promote the same
workflow artifact bytes and must not re-zip the DLL independently.

## Build and Package

After the release integration and version gates pass:

```powershell
cmake -S . -B build/x64/Release -A x64
cmake --build build/x64/Release --config Release --parallel 2
```

Run the project checks available on the release branch, then create the package:

```powershell
.\scripts\package-npp-ai-plugin.ps1 `
  -Platform x64 `
  -Configuration Release `
  -Version 0.2.0.0
```

Expected file:

```text
dist/NppAIAssistant-0.2.0.0-x64.zip
```

Before publication, run the package smoke and release-readiness checks available
on the integrated release branch.

## Publish and Re-verify

After all pre-release gates pass, publish the reviewed commit under tag
`v0.2.0.0` and upload the exact canonical ZIP.

Then download the public asset:

```powershell
$url = 'https://github.com/pingqLIN/NppAIAssistant/releases/download/v0.2.0.0/NppAIAssistant-0.2.0.0-x64.zip'
$out = "$env:TEMP\NppAIAssistant-0.2.0.0-x64.release.zip"
Invoke-WebRequest -Uri $url -OutFile $out
Get-FileHash $out -Algorithm SHA256
```

The downloaded hash is the value that must agree with the canonical pre-upload
hash and the final Plugins Admin `id`.

## Prepare the x64 Entry

Use `plugin-admin-metadata.json` as the project metadata source. For the current
release the final entry should have this shape:

```json
{
  "folder-name": "NppAIAssistant",
  "display-name": "NppAIAssistant",
  "version": "0.2.0.0",
  "id": "<DOWNLOADED_RELEASE_ZIP_SHA256>",
  "repository": "<DIRECT_HTTPS_RELEASE_ZIP_URL>",
  "description": "Lightweight AI assistant plugin for Notepad++ with visible prompts and single-turn behavior.",
  "author": "pingqLIN",
  "homepage": "https://github.com/pingqLIN/NppAIAssistant"
}
```

The `author` value intentionally matches the project metadata and current
published Plugins Admin entry: `pingqLIN`.

## Official Validator and Plugins Admin Local Test

In a fork/branch of `notepad-plus-plus/nppPluginList`:

1. update only the existing NppAIAssistant x64 entry;
2. install the official validator dependencies;
3. run:

```powershell
python -m pip install -r requirements.txt
python validator.py x64
```

Then follow the official Notepad++ manual instructions for local Plugins Admin
testing with a recent portable/debug Notepad++ instance. Verify both install and
update behavior before submitting upstream.

## Submission Gate

The upstream PR may be opened only when all of the following are true:

- [ ] release source commit reviewed and frozen;
- [ ] DLL binary version is `0.2.0.0`;
- [ ] canonical ZIP package accepted;
- [ ] GitHub Release asset is final and directly downloadable;
- [ ] post-download SHA-256 equals canonical SHA-256;
- [ ] final `id` equals that SHA-256;
- [ ] official `validator.py x64` passes;
- [ ] Plugins Admin local install/update test passes;
- [ ] upstream diff changes only the intended x64 JSON entry;
- [ ] user explicitly approves official submission.

Until then, use `docs/NPP_PLUGINLIST_PR_TEMPLATE.md` only as a draft.
# nppPluginList PR Template

Use this only after the final `NppAIAssistant` GitHub Release ZIP has passed the
release gates in `docs/RELEASE_WORKFLOW.md` and the version-specific release
plan. Do not submit this upstream while the release URL, downloaded ZIP
SHA-256, official validator result, Plugins Admin local test, or user approval
is still pending.

## Submission Type

`NppAIAssistant` already has an x64 Plugins Admin entry for version `0.1.0.0`.
The next submission is therefore an **update of the existing x64 entry**, not a
new plugin submission.

## Suggested PR Title

```text
Update NppAIAssistant x64 plugin entry to v0.2.0.0
```

## Suggested PR Body

### Summary

This PR updates the existing `NppAIAssistant` entry in the Notepad++ Plugins
Admin x64 list to version `0.2.0.0`.

### Plugin Overview

- Plugin name: `NppAIAssistant`
- Display name: `NppAIAssistant`
- Version: `0.2.0.0`
- Architecture: `x64`

### Release Information

- GitHub repo: `https://github.com/pingqLIN/NppAIAssistant`
- Release tag: `v0.2.0.0`
- Release asset: `<DIRECT_HTTPS_RELEASE_ZIP_URL>`
- SHA-256: `<DOWNLOADED_RELEASE_ZIP_SHA256>`

### Validation

- DLL binary version: `0.2.0.0`
- Package layout: `NppAIAssistant.dll` at ZIP root
- Official `validator.py x64`: `<PASS>`
- Plugins Admin local install/update test: `<PASS>`
- Release asset re-download hash verification: `<PASS>`

### Entry JSON

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

## Maintainer-facing Checklist

- [ ] Final release tag points to the reviewed release commit
- [ ] Direct HTTPS ZIP URL is anonymously downloadable
- [ ] Downloaded ZIP SHA-256 matches `id`
- [ ] DLL binary version equals `0.2.0.0`
- [ ] DLL is at ZIP root and named `NppAIAssistant.dll`
- [ ] Official `nppPluginList` validator passes for x64
- [ ] Portable/debug Plugins Admin install/update test passes
- [ ] Only the existing `NppAIAssistant` entry in `src/pl.x64.json` is changed
- [ ] User explicitly approved upstream submission

## Stop Condition

If any placeholder remains, any hash differs, the official validator fails, or
the user has not explicitly approved upstream submission, this template stays a
draft and no official PR is opened.

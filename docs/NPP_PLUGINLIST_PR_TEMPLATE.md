# nppPluginList PR Template

Use this as a draft after `NppAIAssistant` has a final GitHub Release ZIP. Do
not submit it while the URL, ZIP SHA-256, or manual package acceptance is still
pending.

## Suggested PR Title

Update NppAIAssistant x64 plugin entry (draft)

## Suggested PR Body

### Summary

This PR updates the existing `NppAIAssistant` entry in the Notepad++ Plugins
Admin list for `x64`. The final entry uses the uploaded ZIP's SHA-256 and direct
download URL.

### Plugin Overview

- Plugin name: `NppAIAssistant`
- Display name: `NppAIAssistant`
- Version: `<DLL_VERSION>`
- Architecture: `x64`

### Project Description

NppAIAssistant is a lightweight AI assistant plugin for Notepad++ with:
- visible prompt preview
- single-turn request behavior
- dynamic model loading
- context menu actions for selected text workflows

### Release Information

- GitHub repo: `https://github.com/pingqLIN/NppAIAssistant`
- Release tag: `<RELEASE_TAG>`
- Release asset: `<DIRECT_HTTPS_ZIP_URL>`

### Packaging Notes

- The DLL name matches the folder name: `NppAIAssistant.dll`
- The zip places the DLL at the root level
- Documentation is included under `doc/NppAIAssistant/`

### Entry JSON

```json
{
  "folder-name": "NppAIAssistant",
  "display-name": "NppAIAssistant",
  "version": "<DLL_VERSION>",
  "id": "<UPLOADED_ZIP_SHA256>",
  "repository": "<DIRECT_HTTPS_ZIP_URL>",
  "description": "Lightweight AI assistant plugin for Notepad++ with visible prompts and single-turn behavior.",
  "author": "NppAIAssistant Contributors",
  "homepage": "https://github.com/pingqLIN/NppAIAssistant"
}
```

### Checklist

- [x] Final GitHub repo URL replaced
- [ ] Final release asset URL replaced
- [ ] SHA-256 updated from final uploaded zip
- [ ] Entry added to the correct architecture file

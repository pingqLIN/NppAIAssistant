# nppPluginList PR Template

Use this when submitting `NppAIAssistant` to the official Notepad++ plugin list.

## Suggested PR Title

Add NppAIAssistant x64 plugin entry

## Suggested PR Body

### Summary

This PR adds the `NppAIAssistant` plugin to the Notepad++ Plugins Admin list for `x64`.

### Plugin Overview

- Plugin name: `NppAIAssistant`
- Display name: `NppAIAssistant`
- Version: `0.1.0.0`
- Architecture: `x64`

### Project Description

NppAIAssistant is a lightweight AI assistant plugin for Notepad++ with:
- visible prompt preview
- single-turn request behavior
- dynamic model loading
- context menu actions for selected text workflows

### Release Information

- GitHub repo: `https://github.com/<owner>/<repo>`
- Release tag: `v0.1.0`
- Release asset:
  `https://github.com/<owner>/<repo>/releases/download/v0.1.0/NppAIAssistant-0.1.0.0-x64.zip`

### Packaging Notes

- The DLL name matches the folder name: `NppAIAssistant.dll`
- The zip places the DLL at the root level
- Documentation is included under `doc/NppAIAssistant/`

### Entry JSON

```json
{
  "folder-name": "NppAIAssistant",
  "display-name": "NppAIAssistant",
  "version": "0.1.0.0",
  "id": "<sha256-of-final-zip>",
  "repository": "https://github.com/<owner>/<repo>/releases/download/v0.1.0/NppAIAssistant-0.1.0.0-x64.zip",
  "description": "Lightweight AI assistant plugin for Notepad++ with visible prompts and single-turn behavior.",
  "author": "NppAIAssistant Contributors",
  "homepage": "https://github.com/<owner>/<repo>"
}
```

### Checklist

- [ ] Final GitHub repo URL replaced
- [ ] Final release asset URL replaced
- [ ] SHA-256 updated from final uploaded zip
- [ ] Entry added to the correct architecture file

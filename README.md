<p align="center">
  <strong>NppAIAssistant</strong><br>
  A lightweight AI assistant plugin for Notepad++
</p>

<p align="center">
  <a href="https://github.com/pingqLIN/NppAIAssistant/releases/tag/v0.1.0"><img src="https://img.shields.io/github/v/release/pingqLIN/NppAIAssistant?label=release" alt="Release"></a>
  <a href="https://github.com/pingqLIN/NppAIAssistant/releases"><img src="https://img.shields.io/github/downloads/pingqLIN/NppAIAssistant/total" alt="Downloads"></a>
  <a href="https://notepad-plus-plus.org/"><img src="https://img.shields.io/badge/platform-Windows-0078D6" alt="Platform"></a>
  <a href="https://notepad-plus-plus.org/"><img src="https://img.shields.io/badge/Notepad++-Plugin-90E59A" alt="Notepad++"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0-blue" alt="License"></a>
  <a href="https://github.com/notepad-plus-plus/nppPluginList/pull/1051"><img src="https://img.shields.io/badge/Plugins%20Admin-PR%20submitted-success" alt="Plugins Admin PR"></a>
</p>

<p align="center">
  <a href="README_zh-TW.md">繁體中文</a>
</p>

---

Visible prompts, modular single-turn profiles, and no hidden memory between requests.

This repository is focused on the plugin itself. It does not carry the full Notepad++ source history, which keeps the project easier to publish, review, package, and release.

## Table of Contents

- [Why It Stands Out](#why-it-stands-out)
- [Screenshots](#screenshots)
- [Build](#build)
- [Install](#install)
- [Repository Layout](#repository-layout)
- [Release and Plugins Admin](#release-and-plugins-admin)

## Why It Stands Out

| Feature | Description |
|---------|-------------|
| **Lightweight** | Ships as a standard Notepad++ plugin — no core fork needed |
| **Prompt visibility** | Live preview of the exact prompt structure in settings |
| **No hidden memory** | Single-turn conversations with no cross-request context |
| **Dynamic models** | Model list loads after provider login or API key setup |
| **Context menu** | Practical right-click actions for editing workflows |
| **Bilingual UI** | English and Traditional Chinese support |

## Screenshots

### Prompt preview in settings

See the exact prompt being built as you adjust presets, language, encoding guidance, and output rules.

<p align="center">
  <img src="docs/assets/screenshots/settings-prompt-preview.png" alt="Prompt Preview" width="680">
</p>

### Preset-driven prompt builder

Switch between presets to quickly configure scenario modules and response parameters.

<p align="center">
  <img src="docs/assets/screenshots/settings-preset-dropdown.png" alt="Preset Dropdown" width="680">
</p>

### Context menu actions

Trigger explanation, refactoring, comments, and fixes directly from the editor.

<p align="center">
  <img src="docs/assets/screenshots/context-menu-actions.png" alt="Context Menu Actions" width="480">
</p>

## Build

<details>
<summary><strong>Visual Studio / MSBuild</strong></summary>

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" `
    "NppAIAssistant.vcxproj" /p:Configuration=Release /p:Platform=x64 /m
```

</details>

<details>
<summary><strong>CMake</strong></summary>

```powershell
cmake -S . -B build
cmake --build build --config Release
```

</details>

Expected output: `build/x64/Release/plugins/NppAIAssistant/NppAIAssistant.dll`

## Install

Copy the built DLL to:

```
<Notepad++>\plugins\NppAIAssistant\NppAIAssistant.dll
```

Or run the install script:

```powershell
scripts/install-npp-ai-plugin.ps1
```

## Repository Layout

```
NppAIAssistant/
├── src/                  # Plugin source, resources, version info
│   └── shared/           # HTTP, provider API, secure storage helpers
├── vendor/
│   ├── notepadpp/        # Vendored plugin & docking headers
│   └── scintilla/include # Scintilla headers for the plugin interface
├── docs/                 # Usage guides, release notes, submission docs
└── scripts/              # Install and package helpers
```

See also: [Project Structure](PROJECT_STRUCTURE.md) · [Usage Guide](docs/USAGE.md) · [Plugins Admin Submission Guide](docs/PLUGIN_ADMIN_SUBMISSION.md)

## Release and Plugins Admin

| Item | Path |
|------|------|
| Packaging script | `scripts/package-npp-ai-plugin.ps1` |
| Metadata | `plugin-admin-metadata.json` |
| Submission guide | [docs/PLUGIN_ADMIN_SUBMISSION.md](docs/PLUGIN_ADMIN_SUBMISSION.md) |

Recommended release config:

- GitHub release tag: `v0.1.0`
- Plugin version: `0.1.0.0`
- Release asset: `NppAIAssistant-0.1.0.0-x64.zip`

---

<p align="center">
  <sub>GPL-3.0 · <a href="https://github.com/pingqLIN/NppAIAssistant">GitHub</a></sub>
</p>

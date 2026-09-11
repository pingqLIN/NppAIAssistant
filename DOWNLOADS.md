# Windows downloads / Windows 安裝包

The project's own distribution location is [GitHub Releases](https://github.com/pingqLIN/NppAIAssistant/releases).
本專案自行維護的安裝包統一放在 GitHub Releases；不必等待 Notepad++ Plugins Admin 更新。

- [Latest stable Windows release / 最新正式版](https://github.com/pingqLIN/NppAIAssistant/releases/latest)
- [All versions and published prereleases / 全部版本與已發布候選版](https://github.com/pingqLIN/NppAIAssistant/releases)

## Available features / 可用功能

Status checked on 2026-09-11. Read each release's asset list and notes before downloading.

| Version | Windows package | OpenRouter | Optional Ctrl + right-click menu |
| --- | --- | --- | --- |
| v0.1.0 (DLL 0.1.0.0) | x64 ZIP available | Not included | Not included |
| main source | No newer published binary yet | Merged in [PR #3](https://github.com/pingqLIN/NppAIAssistant/pull/3), contributed by peppersgc | Pending [PR #6](https://github.com/pingqLIN/NppAIAssistant/pull/6) |
| PR #6 candidate | Windows build and host acceptance pending; no download yet | Included in source | Included in source |

OpenRouter 原始碼已完成整合，但目前 v0.1.0 安裝包不含此功能。新候選包尚未發布，請勿把原始碼下載或開發畫面當成新版安裝包。

## Install / 手動安裝

1. Check **? > Debug Info** in Notepad++ for its architecture. Download the matching ZIP from the release's **Assets** (not GitHub's automatic Source code archives). Only install architectures actually published for that version.
2. If a `.zip.sha256` asset is provided, compare it with `Get-FileHash .\<downloaded-file>.zip -Algorithm SHA256`. Stop if it differs.
3. Close Notepad++. Back up any existing plugin DLL. Extract `NppAIAssistant.dll` into `<Notepad++ directory>\plugins\NppAIAssistant\NppAIAssistant.dll`; a Program Files installation may require administrator access. Do not overwrite your plugin configuration.
4. Restart Notepad++ and open **Plugins > NppAIAssistant**. Use the release notes for supported features. A portable Notepad++ copy is recommended for prerelease testing.

ZIP 是此 Notepad++ 外掛的 Windows 安裝包；不需另外執行 EXE 安裝程式。請配合 Notepad++ 的架構，不是只看 Windows 的架構。若不想手動安裝，可等 Plugins Admin 的條目更新；時間取決於上游審核。

## Maintainer release requirement / 後續發布規則

Every future downloadable release must include at least a validated Windows x64 ZIP and its SHA-256 sidecar. Add Win32 / ARM64 only when built and tested for that release. The ZIP contains the DLL at its root and bundled documentation under `doc/NppAIAssistant/`. Source archives alone are not an installable release.

- Use `scripts/package-npp-ai-plugin.ps1` as the canonical packager. Verify the DLL version matches the package version. Never replace an existing public asset with different bytes; publish a new version.
- Complete Windows build and Notepad++ host checks under [the release workflow](docs/RELEASE_WORKFLOW.md) before publishing. Include source commit, architecture, feature list and known limitations in the release notes.
- When Actions is unavailable, [manual Windows validation](https://github.com/pingqLIN/NppAIAssistant/blob/fix/optional-ai-context-menu-20260911/docs/MANUAL_VALIDATION.md) on the PR #6 branch builds and creates candidate ZIPs outside Actions. Portable Python tests alone do not validate a Windows binary.
- Keep candidate packages on GitHub prereleases. Only accepted stable versions become the latest stable download. Attach the actual ZIP and checksum before publishing; do not promise an asset that is not uploaded.
- Plugins Admin submission is a separate upstream process; this project-owned download location remains available independently.

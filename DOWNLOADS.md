# Windows downloads / Windows 安裝包

The project's own distribution location is [GitHub Releases](https://github.com/pingqLIN/NppAIAssistant/releases).
本專案自行維護的安裝包統一放在 GitHub Releases；不必等待 Notepad++ Plugins Admin 更新。

- [Latest stable Windows release / 最新正式版](https://github.com/pingqLIN/NppAIAssistant/releases/latest)
- [All versions and published prereleases / 全部版本與已發布候選版](https://github.com/pingqLIN/NppAIAssistant/releases)

## Available features / 可用功能

Status checked on 2026-09-26. Read each release's asset list and notes before downloading.

| Version | Windows package | OpenRouter | Optional Ctrl + right-click menu |
| --- | --- | --- | --- |
| v0.2.0.6 (DLL 0.2.0.6) | x64 ZIP available | Included | Included |
| v0.1.0 (DLL 0.1.0.0) | x64 ZIP available | Not included | Not included |

**v0.2.0.6 is the current GitHub release.** The Notepad++ Plugins Admin entry may still show the previous version until [upstream Plugin List PR #1196](https://github.com/notepad-plus-plus/nppPluginList/pull/1196) is accepted.

Direct x64 asset:
- `NppAIAssistant-0.2.0.6-x64.zip`
- SHA-256: `23B5051C584FE331C150D3361FC6FE4A23838CCDD33194F75A544776AC76B621`
- Checksum sidecar is published beside the ZIP in GitHub Releases.

v0.2.0.6 已正式發布，可直接從 GitHub Releases 下載；Plugins Admin 更新則需等待上游 Plugin List 審核。

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

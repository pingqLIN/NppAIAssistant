# NppAIAssistant

A Windows Notepad++ plugin for explicit, single-turn AI assistance while you
edit. It keeps the prompt-building controls visible, lets you work from the AI
panel or a deliberate context-menu gesture, and does not retain hidden chat
memory between requests.

[繁體中文](README_zh-TW.md)

## Start here

NppAIAssistant is source-buildable as a standalone C++20 plugin. Build an x64
DLL with CMake:

```powershell
cmake -S . -B build-cmake
cmake --build build-cmake --config Release
```

Copy the resulting `NppAIAssistant.dll` into:

```text
<Notepad++>\plugins\NppAIAssistant\NppAIAssistant.dll
```

Restart Notepad++, open **Plugins > NppAIAssistant > Settings**, configure a
provider, test its connection, and explicitly choose a discovered model before
sending a request. See the [usage guide](docs/USAGE.md) for the complete setup
and safety notes.

## What it provides

- A docked AI panel for one-off questions and responses.
- Visible single-turn prompt profiles: presets, language and encoding guidance,
  detail level, scenario modules, output rules, and a prompt preview.
- Editable Identity, Rules, and Assignment sections, plus optional visible
  Memory. Memory is off by default and is never hidden context.
- Dynamic model discovery for OpenAI, Gemini, Claude, and an optional local
  OpenAI-compatible `/v1` server.
- Model safety: a missing saved model is not silently replaced. Choose a model
  again before sending.
- Editor actions for explaining, refactoring, commenting on, or fixing selected
  text. The AI menu opens only for **Ctrl + right-click** on a non-empty mouse
  selection; normal and keyboard context menus remain with Notepad++.
- English and Traditional Chinese UI text.

## Privacy and connection boundaries

API keys are protected with Windows DPAPI in local application storage.
Non-secret preferences, such as prompt templates, selected models, timeout, and
the optional local endpoint, are stored in the Notepad++ plugin configuration
file under `%AppData%`.

The optional local OpenAI-compatible provider accepts only literal loopback
`/v1` endpoints: `http://127.0.0.1:<port>/v1` or
`http://[::1]:<port>/v1`. `localhost` is normalized to `127.0.0.1`. Requests to
this provider bypass proxies, refuse redirects, and do not accept remote hosts.

The request timeout defaults to 30 seconds and can be set from 1 to 300 seconds
in Settings. Each request captures its own timeout, endpoint, model, and
credential state before it begins.

Do not place passwords, customer data, API keys, or other secrets in prompt
templates or the optional Memory field; those are ordinary plugin preferences.

## Documentation

- [Usage guide](docs/USAGE.md) — installation, first-time setup, prompt tools,
  context-menu behaviour, and troubleshooting.
- [Local provider and timeout](docs/LOCAL_PROVIDER_AND_TIMEOUT.md) — loopback
  endpoint policy, model-selection policy, and manual acceptance checks.
- [Project change log](docs/CHANGELOG.md) / [專案更新紀錄](docs/CHANGELOG.zh-tw.md)
  — user-facing summary of the current unreleased changes.
- [Plugins Admin submission guide](docs/PLUGIN_ADMIN_SUBMISSION.md) — packaging
  and official-list prerequisites. It does not mean the plugin is listed.
- [Project structure](PROJECT_STRUCTURE.md) — source and packaging layout.

## Verification and release status

The repository includes a focused security regression script and package
readiness tooling:

```powershell
.\scripts\verify-security-regressions.ps1
.\scripts\package-npp-ai-plugin.ps1 -Platform x64
.\scripts\smoke-package-install.ps1
.\scripts\verify-release-readiness.ps1 -Platform x64 -Configuration Release
```

The code build and static checks do not replace manual Notepad++ acceptance.
Before a public release, verify the packaged ZIP in Notepad++, including the
settings dialog, context-menu gesture, local-endpoint rejection, model selection,
and unload behaviour.

NppAIAssistant already has an x64 Plugins Admin entry for version `0.1.0.0`.
This branch's source changes are not an update to that published package. A
future update still requires a new DLL version, a final GitHub Release ZIP at a
direct HTTPS URL, its final SHA-256, an updated architecture-specific entry,
and the upstream maintainers' review.

## Contributing and license

Issues and pull requests are welcome at
[pingqLIN/NppAIAssistant](https://github.com/pingqLIN/NppAIAssistant). Please do
not include secrets, local API keys, or generated build output in a contribution.

This repository is distributed under the [GNU GPL version 3](LICENSE).

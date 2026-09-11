<p align="center">
  <img src="docs/assets/readme/nppaiassistant-banner.png" alt="An original fox editing companion presenting blank document cards" width="100%">
</p>

# NppAIAssistant

**AI assistance beside your text. Visible prompts. Deliberate edits.**

A native Notepad++ plugin for explaining code, rewriting text, and turning a request into a useful draft—using a cloud provider or a local model.

[繁體中文](README_zh-TW.md) · [Download](https://github.com/pingqLIN/NppAIAssistant/releases/latest) · [Report an issue](https://github.com/pingqLIN/NppAIAssistant/issues) · [GPL-3.0](LICENSE)

> **Version guide:** the published x64 release and official Plugin List entry are **0.1.0.0** (`v0.1.0`). The workspace tour below shows the **0.2.0.6 candidate**, which is not yet a published download. These new screenshots show actual native dialogs with sample content in an isolated UI preview; they do not demonstrate a live model response or a full Notepad++ host test.

> **Development source:** OpenRouter is already included in `main` (PR #3). The optional AI context menu uses Ctrl + mouse right-click on selected text; ordinary right-click and keyboard menus stay native. Disable it in Settings. This behavior is not in the published v0.1.0 download. See [usage](docs/USAGE.md#context-menu-actions).

## From a question to an edit

1. **Choose a service and model.** Use your preferred cloud API or a local LM Studio endpoint.
2. **Shape the request.** Select a task preset and output format, then inspect the prompt before sending.
3. **Read the result.** Switch between a basic formatted preview and the original response.
4. **Choose where it goes.** Keep the answer in the panel, insert at the cursor, replace the selection, or create a new document.

## Meet the workspace · 0.2.0.6 preview

### Keep the request, response, and destination together

<img src="docs/assets/screenshots/workspace-0.2.0.6-en.png" alt="English native workspace preview showing provider controls, a sample Markdown response, output destination and input box" width="680">

The upper controls select the provider, model, task profile, and output format. The response occupies the center; the destination and input remain below it. Resize the panel, drag the input divider, or use **A+ / A−** to make the text comfortable to read.

| Control | What it helps you do |
| --- | --- |
| Preview before send | Check the assembled prompt before it reaches the selected provider. |
| Formatted / original view | Read basic Markdown or indented JSON while retaining the original response. |
| Output destination | Explicitly choose panel, cursor, selection, or new document. |
| Reply selection | Choose which assistant reply to insert. |
| New / branch conversation | Organize local transcripts. Conversation branches do not automatically become model context. |

### Configure the connection separately from the task

<img src="docs/assets/screenshots/settings-0.2.0.6-en.png" alt="Native settings dialog with empty API key fields and local provider settings" width="680">

The settings separate provider connections from prompt configuration. LM Studio has its own base URL, API mode, and model selection. Discover available models, then explicitly choose a default model. API keys in this screenshot are empty. Model discovery is unavailable in this network-disabled screenshot fixture.

### Know what you are sending

<img src="docs/assets/screenshots/prompt-0.2.0.6-en.png" alt="Native prompt settings showing task presets, output rules and prompt preview" width="680">

Prompt configuration brings task presets, response language, output rules, and the assembled preview into one place. Built-in template sections are locked by default and require an explicit unlock to edit. Optional visible Memory is disabled by default; it is ordinary local text, so keep secrets out of it.

## Providers and output

The candidate implements OpenAI, Gemini, Claude, LM Studio, and a generic OpenAI-compatible profile. Availability, model access, and usage charges depend on the selected provider. Copilot is currently paused.

| Output mode | Behavior in the candidate |
| --- | --- |
| Text | Plain response for general editing and drafting. |
| Markdown | Basic headings, emphasis, code, lists, and quotes in the panel. HTML, images, links, and tables are not rendered. |
| JSON | Asks for JSON; this alone is not schema enforcement. |
| Structured JSON | Uses native schema transport and local validation for supported OpenAI and LM Studio Chat Completions models. Unsupported routes are blocked. |

Requests are non-streaming: the status strip reports request phases, and the answer appears after the response arrives. In **0.2.0.6**, local loopback generation uses a **900-second timeout for the relevant HTTP phases**; this is not a 900-second total request deadline. Model discovery uses a separate short timeout.

English and Traditional Chinese are supported. Japanese and Spanish cover the main workspace, with English fallback in advanced settings.

## Install the published release

Use **Plugins → Plugins Admin**, search for **NppAIAssistant**, and install the entry offered by your Notepad++ Plugin List. List updates may reach installations at different times.

For manual installation on **x64 Notepad++**:

1. Download `NppAIAssistant-0.1.0.0-x64.zip` from the [v0.1.0 release](https://github.com/pingqLIN/NppAIAssistant/releases/tag/v0.1.0).
2. Close Notepad++ and back up any existing plugin DLL.
3. Extract `NppAIAssistant.dll` to `<Notepad++>\plugins\NppAIAssistant\NppAIAssistant.dll`.
4. Restart Notepad++ and open its **Plugins → NppAIAssistant** menu.

The candidate screenshots above will differ from the published version. Match the plugin architecture to your editor; this page does not offer x86 or ARM64 release downloads.

## Privacy and editing behavior

- A request sends its assembled prompt and included text to the endpoint you choose. A local endpoint keeps that request local only if the configured service itself runs locally.
- The candidate protects stored API credentials with Windows DPAPI under `%LocalAppData%\Notepad++\AIAssistant`. Preferences and visible prompt text live under `%AppData%\Notepad++\plugins\config\NppAIAssistant.ini`.
- A portable Notepad++ folder does **not** isolate those production settings paths.
- Editor writes are guarded against changed documents, read-only buffers, and lossy encoding conversion. Review generated text before applying it; supported writes are grouped for undo.
- Requests are single-turn by default. A visible transcript does not mean previous replies are automatically sent again.

## Build and contribute

Use Windows, Visual Studio with the C++ workload and Windows SDK, and CMake 3.21 or newer. From a source checkout:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Build output paths depend on the checked-out revision and generator. Use the repository packaging scripts when preparing a distributable ZIP. For bug reports, include plugin and Notepad++ versions, architecture, provider/API mode, and a minimal reproducible example with credentials and private text removed.

The [visual design notes](docs/VISUAL_DESIGN.md) explain the original AI-generated banner and screenshot provenance. The banner is project artwork; interface screenshots are captured native controls. This project is developed with AI assistance and distributed under [GPL-3.0](LICENSE).

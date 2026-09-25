# NppAIAssistant 0.2.0.6 (Windows x64)

An AI workspace inside Notepad++ with visible prompts, local LM Studio and
OpenAI-compatible services, basic Markdown/JSON display, and editor output.

Close Notepad++ before installing. Extract NppAIAssistant.dll to
`plugins/NppAIAssistant/NppAIAssistant.dll` under an x64 Notepad++ installation.
Preserve your previous DLL for rollback. Restart and open the plugin workspace.
Review the selected service, model, and prompt before sending content.

Local generation uses a 900-second WinHTTP phase timeout; discovery uses 1.5
seconds. This is not a total wall-clock deadline. Remote service limits remain
unchanged. A model/provider must be available; hosted providers can incur costs.

Requests send the visible prompt and selected context to the chosen service.
API keys are protected with Windows DPAPI in the current user's LocalAppData;
preferences and optional memory are plain text in AppData. Portable Notepad++
does not isolate these storage locations. Do not store secrets in prompt memory.

This package targets x64 only. Other architectures and a minimum supported
Notepad++ version are not certified by this candidate. Japanese and Spanish
cover the main workspace, with English fallback in advanced settings.

See USAGE.md, WORKBENCH.md and EDITOR_OUTPUT.md for controls and limitations.
Source, license and issue reporting: https://github.com/pingqLIN/NppAIAssistant
License: GPL-3.0-or-later; see LICENSE.

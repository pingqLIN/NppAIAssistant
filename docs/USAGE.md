# Usage Guide

## What This Plugin Is Good At

`NppAIAssistant` is built for quick, explicit, editor-side AI assistance inside Notepad++.

Best fit scenarios:
- explain selected code
- refactor a selected block
- add comments to selected code
- fix a selected block
- ask one-off implementation questions in the AI panel

It is intentionally not designed as a long-memory chat workspace. Each request is treated as independent.

## Install into Notepad++

### Manual install
1. Build the plugin DLL.
2. Create this folder if it does not exist:
   `<Notepad++>\plugins\NppAIAssistant\`
3. Copy:
   `NppAIAssistant.dll`
4. Restart Notepad++.

### Helper script
You can also use:
`scripts/install-npp-ai-plugin.ps1`

Default script assumptions:
- Notepad++ is installed in `C:\Program Files\Notepad++`
- the built DLL is in the repo `build/` output tree

## First-Time Setup

1. Open the plugin settings.
2. Enter an API key for OpenAI, Gemini, or Claude.
3. Choose the default provider.
4. Click `Test Default Connection`.
5. Confirm models are loaded dynamically for that provider.
6. Choose the UI language if needed.

## Settings and Secret Storage

- API keys and OAuth tokens are stored in `%LocalAppData%\Notepad++\AIAssistant`.
- Secret values are protected with Windows DPAPI.
- Non-secret preferences are stored in `%AppData%\Notepad++\plugins\config\NppAIAssistant.ini`.
- Older roaming secure blobs are migrated automatically on first launch of the updated build.

## Prompt Builder

The settings window includes a single-turn prompt builder.

You can control:
- preset
- response language
- encoding suggestion
- response detail
- scenario modules
- output rules
- identity, rules, and assignment templates through `Prompt Sections...`

The prompt preview updates as these options change. It now shows the prompt as
visible sections:

- Mandatory System
- Identity
- Rules
- Memory
- System
- Assignment
- User Request

`Mandatory System` is not user-editable. It preserves the single-turn and
no-hidden-memory contract even when other prompt templates are changed.

The preview also includes per-section and total estimated tokens. These values
are local estimates, not exact provider billing or context-window counts.

Use `Prompt Sections...` to edit the visible Identity, Rules, and Assignment
templates. The token dropdown inserts supported variables such as
`{{PROVIDER}}`, `{{MODEL}}`, `{{LANGUAGE}}`, and `{{TIMESTAMP}}` into the
focused template editor. Reset buttons restore the built-in defaults. Pressing
`OK` in this dialog saves the non-secret prompt-section preferences immediately
through the plugin settings file.

## Presets

Current presets are designed for fast one-off tasks:
- Custom
- Code Fix
- Refactor
- Explain Code
- Generate Tests
- Write Docs

These presets do not add hidden memory. They only reshape the prompt for the current request.

## Memory Storage

Memory storage is explicit and disabled by default. Use `Memory...` in settings
to enable a visible Memory section and edit the memory text that will be sent.
The prompt preview shows whether Memory is off or how many estimated tokens it
adds.

Memory is stored as plain plugin settings, not DPAPI-protected secret storage.
Do not store API keys, OAuth tokens, passwords, private customer data, or
anything that should not appear in normal Notepad++ plugin configuration files.

## Capability Boundaries

These items are planned but not shipped in the current build:

- real OAuth sign-in for OpenAI, Gemini, Claude, or a hosted broker
- floating inline editor overlay

The existing paused Copilot OAuth code is not the same as a shipped OAuth login
feature. OAuth work must be reported separately as storage model implemented,
provider UI prepared, and real provider sign-in validated.

## Waiting State

Provider requests run off the UI thread. While a request is active, the panel
shows a timer-driven `Waiting for AI response...` message and disables provider,
model, and send controls. When the request completes, the result is posted back
to the UI thread before the chat display or selected editor text is updated.

## Custom Context Templates

Use `Context Templates...` in settings to configure up to three right-click
templates. Each slot has:

- enable toggle
- menu name
- prompt template
- optional replacement mode

Enabled templates appear under the standard AI context-menu actions when text is
selected in Notepad++. Replacement mode writes the AI result back to the original
selection after the async request completes and the original buffer is still
active.

## Prompt Preview

The `Prompt Preview` area shows the effective prompt structure that will be sent for a request.

What it helps with:
- checking whether the language is correct
- checking whether output should be concise or detailed
- checking whether the prompt is optimized for fix, refactor, tests, or docs
- understanding what the plugin is really sending

## Single-Turn Behavior

This plugin is intentionally single-turn.

That means:
- no long-running hidden memory
- each request stands on its own
- safer and easier prompt inspection
- easier to predict why a response was generated

## Context Menu Actions

After selecting text in Notepad++, you can use the AI context menu actions:
- AI: Explain Selection
- AI: Refactor Selection
- AI: Add Comments
- AI: Fix Selection

These actions are optimized for fast in-editor use.

## AI Panel Workflow

1. Open the AI panel.
2. Pick provider and model.
3. Type a request.
4. Send.
5. Review the formatted response.

If `Ctrl+Enter` mode is enabled, plain Enter will no longer send directly.

Use `A+` and `A-` in the panel toolbar to adjust display scale for the chat and
input text. The scale is stored as a non-secret preference and is clamped between
80% and 150%.

## Model Loading

Models are loaded dynamically after the relevant provider is configured and available.
This helps avoid stale hardcoded model lists and makes the plugin better aligned with the actual provider account state.

## Recommended GitHub Demo Flow

If you are preparing screenshots or a short demo:
1. Show the settings dialog with prompt preview
2. Show preset switching
3. Show the right-click AI actions
4. Show a single request in the AI panel
5. Highlight that the system is lightweight and single-turn

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

## Prompt Builder

The settings window includes a single-turn prompt builder.

You can control:
- preset
- response language
- encoding suggestion
- response detail
- scenario modules
- output rules

The prompt preview updates as these options change.

## Presets

Current presets are designed for fast one-off tasks:
- Custom
- Code Fix
- Refactor
- Explain Code
- Generate Tests
- Write Docs

These presets do not add hidden memory. They only reshape the prompt for the current request.

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

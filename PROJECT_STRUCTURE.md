# Project Structure

This repository is organized as a standalone Notepad++ plugin project.

## Top Level

- `src/`
  Main plugin source, dialog resources, and version metadata.
- `src/shared/`
  Shared helper modules for HTTP requests, provider API calls, and secure local storage.
- `vendor/notepadpp/`
  Vendored Notepad++ headers required to build the plugin independently from the full upstream tree.
- `vendor/scintilla/include/`
  Vendored Scintilla headers used by the Notepad++ plugin interface.
- `docs/`
  Public documentation, screenshots, release notes, and Plugins Admin submission notes.
- `scripts/`
  Helper scripts for local installation and release packaging.
- `plugin-admin-metadata.json`
  Metadata source for package generation and `nppPluginList` style output.
- `NppAIAssistant.vcxproj`
  Standalone Visual Studio project for x64 builds.
- `CMakeLists.txt`
  Standalone CMake build entry point.

## Source Layout

- `src/NppAIAssistant.cpp`
  Main plugin entry point, docked AI panel, settings dialog, prompt builder, i18n, and context-menu workflow.
- `src/NppAIAssistantResources.h`
  Resource control IDs.
- `src/NppAIAssistantResources.rc`
  Dialog layout, UI strings, and version resource binding.
- `src/NppAIAssistantVersion.h`
  File version metadata used by the DLL and Plugins Admin packaging flow.

## Shared Helpers

- `src/shared/HttpClient.*`
  WinHTTP wrapper utilities.
- `src/shared/LLMApiClient.*`
  Provider request and model-list integrations.
- `src/shared/SecureStorage.*`
  Local secure storage for API keys and related settings.

## Packaging

- `scripts/package-npp-ai-plugin.ps1`
  Builds a release zip layout for Notepad++ Plugins Admin style distribution.
- `scripts/install-npp-ai-plugin.ps1`
  Copies the built DLL into a local Notepad++ installation.
- `docs/PLUGIN_ADMIN_SUBMISSION.md`
  Submission checklist and release workflow notes.

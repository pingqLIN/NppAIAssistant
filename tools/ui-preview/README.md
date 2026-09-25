# Native panel preview

This separate CMake target compiles the real Win32 panel and resources, opens
the panel on Notepad++ readiness, and substitutes memory-only preferences,
disabled credential storage, and blocked HTTP adapters. It is a visual preview,
not an AI service or release acceptance build. Never distribute its DLL as a
production release.

Build this directory as the CMake source in a separate build directory, then run
CTest. Use a fresh portable Notepad++ directory with only this preview plugin;
do not copy existing user configuration, sessions, or plugin directories.

The regular repository build and its release packages are unaffected.

## Connected test profile

Configure a separate build directory with `-DNPPAI_PREVIEW_ENABLE_NETWORK=ON`
to use production HTTP for LM Studio and OpenAI-compatible services. The title
identifies this as Connected Test. Settings remain in memory; keys entered by
the user are retained as DPAPI-protected blobs in this process only. Existing
credential paths are never accessed. This DLL is still not a release artifact.

For LM Studio, use `http://127.0.0.1:1234/v1`, discover and explicitly select a
model in Settings, then confirm. Use the external provider's base URL and key
for other compatible services; do not enter a full `/chat/completions` endpoint.

# Local Provider and Request Timeout

## Delivered implementation plan

This change implements four coordinated behaviours:

1. A request timeout setting, expressed in seconds, defaults to `30` and only
   accepts whole values from `1` through `300`.
2. Each model-discovery and inference request receives its own timeout snapshot.
   Saving a new setting cannot change the timeout of an already-running request.
3. The optional OpenAI-compatible provider is limited to a local `/v1` service.
   The accepted forms are `http://127.0.0.1:<port>/v1` and
   `http://[::1]:<port>/v1`. `localhost` is accepted on input and canonicalized
   to `127.0.0.1`; remote host names, remote IPs, redirects, proxies, query
   strings, credentials in the URL, and non-`/v1` paths are rejected.
4. A discovered model is used only after an explicit selection. If a persisted
   model disappears, the model field is left unselected and the request is
   blocked; the plugin never substitutes the first available model.

## Operator use

Open **Plugins > NppAIAssistant > Settings**. Set **Request timeout (seconds)**
to the desired value, then save. Invalid input remains in the dialog and cannot
be saved. The setting applies to OpenAI, Gemini, Claude, local-compatible model
listing, connection tests, and inference.

To use a local OpenAI-compatible server, enter its local base URL in the form
above, select **Local OpenAI-compatible** as the provider, test the connection,
then explicitly select one of the returned models. The local API key is optional
and is stored with Windows DPAPI secure storage; the endpoint, timeout, and
selected model are ordinary plugin preferences.

## Context-menu preflight

The AI context menu is shown only for a mouse-invoked Ctrl+right-click on a
non-empty Scintilla selection. Ordinary right-clicks, right-clicks without a
selection, and keyboard context-menu invocations continue to Notepad++'s native
handler. The hook consumes the message only after its AI popup is created.

## Verification and remaining acceptance

Run `scripts\verify-security-regressions.ps1` and build the DLL with CMake.
Manual Notepad++ acceptance should cover native and Ctrl context menus, timeout
save/reopen behaviour, rejected endpoint forms, a missing persisted model, and
local model discovery with a deliberately configured service. This repository
does not contain an automated Notepad++ UI test harness, so those interactive
checks remain a release gate.

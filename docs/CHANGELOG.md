# Project Change Log

## Unreleased

### Safer AI connection controls

- Added a request-timeout field in Settings. It defaults to 30 seconds, accepts
  values from 1 through 300, and is captured for each discovery or inference
  request.
- Added an optional `Local OpenAI-compatible` provider for literal loopback
  OpenAI-compatible `/v1` endpoints only. Local requests bypass proxies, refuse
  redirects, and reject remote hosts.
- Saved models are now restored only when the exact model remains available.
  Missing models require an explicit new selection instead of a silent fallback.

### Editor interaction

- The AI editor menu now requires a mouse Ctrl+right-click on a non-empty
  selection. Normal right-click and keyboard context-menu behaviour stays with
  Notepad++.

### Documentation and validation

- Reworked the README and usage guidance around verified setup, privacy
  boundaries, and the actual release state.
- Extended the security regression check to cover timeout settings, loopback
  transport restrictions, explicit model selection, and context-menu gating.

## Release note status

These are unreleased source changes. A GitHub Release ZIP, final SHA-256, and
manual Notepad++ package acceptance are still required before an official
Plugins Admin submission.

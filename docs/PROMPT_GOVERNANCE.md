# Prompt Policy and Runtime Policy

## Status

- **VERIFIED:** NppAIAssistant is a single-turn editor assistant. It does not
  expose model-invoked file, shell, network, MCP, or workflow tools.
- **VERIFIED:** Provider transport, structured-output capability checks, JSON
  parsing, schema validation, response diagnostics, and selection replacement
  are runtime code paths. A user editing Task Instructions cannot remove them.
- **VERIFIED:** Prompt source data and optional local memory are labelled as
  untrusted data in the assembled request.
- **NOT VERIFIED:** Prompt wording alone prevents every prompt injection. The
  runtime has no general-purpose tool execution surface to protect in this
  release.

## Boundary

| Layer | Responsibility | Enforcement |
| --- | --- | --- |
| Runtime Policy | Provider requests, timeouts, structured validation, editor writes, redaction | C++ runtime |
| Prompt Policy | Single-turn disclosure, evidence limits, honesty, untrusted-data handling | Assembled prompt; editable only after an explicit unlock |
| Task Profile | General, Review, Transform, Code Change, Documentation | Prompt assembly |
| Request Data | Task Instructions, selection, visible local memory | User data; never policy |

`Prompt Policy` is deliberately short. It instructs the model to use visible
data, state material unknowns, avoid claims of unavailable actions, preserve
the source boundary, follow the runtime-selected output contract, and prefer a
smallest correct editor transformation.

## Prompt section locks

The built-in Prompt Policy, Identity, Rules, Assignment, Scenario Modules, and
Output Contract sections are **locked by default**. In Prompt Sections, clear
**Lock section** to edit a section; its content and lock state are saved only
when the containing Settings dialog is confirmed with **OK**. Reset restores
the generated or built-in default for that section, but does not change its
lock state.

Runtime Context, current Task Instruction, selected Source Context, and visible
Memory remain data or runtime snapshots rather than editable templates. Unlocking
a prompt section never grants tools, bypasses provider capability checks, removes
Structured JSON transport/schema validation, or changes editor-write safeguards.

## Composer behavior

The Composer exposes an editable **Task Instructions** field and a read-only
runtime summary. On Send, the plugin assembles the selected policy, output
contract, runtime context, task profile, optional memory, edited task, and
source data. The editor does not expose the final wire prompt as an editable
security control.

Structured JSON remains a transport-layer data contract. Its schema is not
inserted into Task Instructions and is still validated after response
extraction.

## Panel controls and request visibility

The assistant panel exposes the selected Task Profile, Output Mode, send-preview
preference, and two constrained prompt compositions:

- `Runtime -> Policy -> Profile -> Memory -> Task -> Source (cache-first)` is
  the default. Its stable policy/profile prefix may be reusable by providers that implement compatible
  prompt caching. This is **INFERRED**, not a provider cache guarantee.
- `Runtime -> Policy -> Profile -> Task -> Memory -> Source` is the alternate
  request-data order. It never moves task, memory, or source data before the
  governed policy/profile prefix.

Both layouts keep Runtime Policy, Prompt Policy, Task Profile, and Output
Contract before all request data.
Runtime behaviour and the Structured JSON schema are not movable prompt
sections. The panel's status strip distinguishes local preparation, HTTP
transport connection, sending, awaiting a provider response, receiving, local
parsing/validation, completion, failure, and preview cancellation. It records
only state labels: it never records prompt content, endpoint URLs, headers,
credentials, or model output.

Send Preview is enabled by default. When it is disabled, the same prompt
assembler and provider-neutral Structured Output configuration are used; only
the editable Composer dialog is skipped. Cancelling the enabled preview starts
no request.

`New` and `Branch` are local transcript operations. A branch copies visible
panel messages for review but does not automatically transmit any prior
messages to a provider. This preserves the verified single-turn contract.

## Source data and diagnostics

Selected document content is emitted as:

```xml
<source_context trust="untrusted">
…selected document content…
</source_context>
```

Visible memory uses the same boundary. The explicit task may ask the model to
analyze or adopt a source instruction, but a source instruction cannot itself
replace the Prompt Policy.

`PromptAssemblyManifest` is in-memory metadata for UI/diagnostic summaries. It
contains policy version, task profile, output mode, validation state, section
IDs, included state, source trust, character counts, and token estimates. It
does not retain prompt, selected text, memory, provider response, or API-key
payloads.

## Future work

Tool execution, autonomous retries, long-term memory, multi-agent handoffs,
MCP, and an AGENTS.md loader are intentionally out of scope. If an actual tool
surface is introduced, add capability allowlists, pre-execution approval, tool
input/output validation, and an auditable execution record before exposing it
to a model.

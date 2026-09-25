# Provider API compatibility

This note records the transport contract used by NppAIAssistant. Transport
format is implemented by the API client; prompt templates describe only the
assistant answer that should appear in Notepad++.

## Supported baseline

| Service | Base URL | Authentication | Text endpoint | Response text |
| --- | --- | --- | --- | --- |
| OpenAI | `https://api.openai.com/v1` | `Authorization: Bearer <token>` | `POST /chat/completions` | `choices[0].message.content` |
| LM Studio | `http://127.0.0.1:1234/v1` by default | Optional local API token | `POST /chat/completions` or `POST /responses` | Chat content or Responses `output_text` content |
| Hugging Face Inference Providers | `https://router.huggingface.co/v1` | `Authorization: Bearer <HF_TOKEN>` | `POST /chat/completions`; `/responses` is beta | Chat content or Responses `output_text` content |

All OpenAI-compatible and Copilot chat requests set `stream: false`, because
those client paths consume a complete JSON response and do not parse
Server-Sent Events. Gemini and Claude use their provider-specific non-streaming
request endpoints rather than an OpenAI-compatible `stream` field. The generic
OpenAI-compatible profile can select Chat Completions or Responses. A Hugging
Face model may include a routing suffix such as `:fastest`, `:cheapest`,
`:preferred`, or a provider name.

The canonical Chat Completions request used by the compatible profile is:

```json
{
  "model": "provider-model-id",
  "messages": [{ "role": "user", "content": "assembled prompt" }],
  "stream": false
}
```

The client reads the assistant text from
`choices[0].message.content`. It retains a bounded fallback for older compatible
servers that use `choices[0].text`.

The canonical Responses request is:

```json
{
  "model": "provider-model-id",
  "input": "assembled prompt",
  "store": false,
  "stream": false
}
```

The client joins `output` content items whose `type` is `output_text`.

## Prompt/output boundary

The default Output Contract asks the model to return the answer itself, without
an OpenAI/LM Studio/Hugging Face transport envelope. If a task explicitly asks
for JSON, it asks for exactly one JSON value without Markdown fencing. This is
prompt-level guidance, not schema enforcement.

Strict structured output must be configured in the HTTP request. For Chat
Completions, OpenAI and LM Studio use `response_format.type = json_schema` with
a JSON schema. Schema JSON is never copied into the assembled prompt.

## Structured Outputs (Phase A+B)

The Settings Prompt tab provides `Text`, `Markdown`, `JSON`, and `Structured
JSON` modes. `JSON` is prompt-only guidance. `Structured JSON` uses a
provider-native schema transport plus a local JSON/schema validation boundary.
The initial UI deliberately offers only bounded built-in presets (`Generic
Structured Result` and `Document Review`); a free-form schema editor is not in
this phase.

| Provider / endpoint | Capability | Request shape | Evidence status |
| --- | --- | --- | --- |
| OpenAI Chat Completions | Native (supported models only) | `response_format.json_schema` | VERIFIED (official format); live credential test NOT VERIFIED |
| OpenAI Responses | Native (supported models only) | `text.format` | VERIFIED (official format); adapter path is not exposed by the current OpenAI settings UI |
| LM Studio Chat Completions | Native transport (model-dependent) | `response_format.json_schema` | VERIFIED (official format and bounded local live test on 2026-08-22) |
| LM Studio Responses | Unknown / blocked | no schema payload is sent | NOT VERIFIED: LM Studio structured-output document covers Chat only |
| Generic OpenAI-compatible | Unknown / blocked | no schema payload is sent | INFERRED: endpoint/model capabilities are not knowable from a generic profile |
| Hugging Face aggregate router | Unknown / blocked | no schema payload is sent | INFERRED: downstream provider and model may vary |
| Gemini, Claude, Copilot | Unsupported / blocked | no schema payload is sent | VERIFIED for this plugin implementation boundary |

Native Chat payload (schema appears once as an object, and `strict` is a JSON
boolean):

```json
{
  "response_format": {
    "type": "json_schema",
    "json_schema": { "name": "structured_result", "strict": true, "schema": { } }
  }
}
```

Native Responses payload uses a different wrapper:

```json
{
  "text": {
    "format": { "type": "json_schema", "name": "structured_result", "strict": true, "schema": { } }
  }
}
```

The local validator implements a project-specific closed subset; it does **not**
claim Draft 2020-12 or any other complete JSON Schema draft compliance.
Unsupported keywords fail closed at every nested schema node. The supported
keywords are `type`, `properties`, `required`, `additionalProperties`, `items`,
`enum`, `const`, `minimum`, `maximum`, `minLength`, and `maxLength`. `type` must
be one supported type name (type arrays/unions are not supported),
`additionalProperties` must be boolean, and `items` must be one schema rather
than tuple form. JSON numbers are compared as exact base-10 values; exponent
magnitude is bounded to 1,000,000 and larger values fail closed. A response
is usable only after transport, documented content extraction, JSON parsing,
and (when enabled) schema validation all succeed. `finish_reason: "length"`
and Responses `status: "incomplete"`, `queued`, `in_progress`, or `cancelled`
are reported as incomplete generation, not JSON parse errors; other non-stop
Chat finish reasons and provider refusals are blocked. `Native` in the matrix
means the endpoint transport shape is documented, not that every selected model
has been live-verified; provider rejection remains a typed error with no
fallback. Raw extracted model content is retained for validation diagnostics,
never request headers or API keys; a failed structured response cannot replace
editor selection.

OpenAI documents Structured Outputs support for `gpt-4o-mini`,
`gpt-4o-mini-2024-07-18`, `gpt-4o-2024-08-06`, and later compatible models.
Older or otherwise unsupported models may reject the native request. LM Studio
also documents model-dependent reliability (especially for smaller models), so
the plugin never converts a provider rejection into a silent prompt-only
fallback.

The 2026-08-22 loopback snapshot used the already-loaded
`openai/gpt-oss-20b` model at `http://127.0.0.1:1234/v1` without an
Authorization header. A valid schema, integer type enforcement, required
fields, enum enforcement, prompt-only JSON, and Text requests returned HTTP
200 with the expected output. An invalid schema returned HTTP 400, and
`max_tokens: 1` returned `finish_reason: "length"`. This is a bounded local
verification of that model/server snapshot, not a capability claim for every
LM Studio model.

On 2026-08-23, a separate no-credential loopback smoke verified that the same
server exposed seven models through `GET /v1/models`. With
`openai/gpt-oss-20b`, Chat Completions with `stream: false` and a sufficient
completion budget returned `finish_reason: "stop"` and `local-ok`; a native
`response_format.type: "json_schema"` request returned a JSON object with the
required `result` property. A deliberately tiny eight-token budget returned
`finish_reason: "length"` with no answer content. These are **VERIFIED** local
snapshot observations, not a throughput, token-cost, or all-model guarantee.

Loopback model discovery uses a short 1.5-second bound, while generation uses
a separate five-minute timeout. Local inference must not inherit the discovery
timeout: larger CPU/GPU-offloaded models can legitimately need more than 30
seconds to produce a complete non-streaming response.

## Official references

- OpenAI Chat Completions API: <https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/create>
- OpenAI model guidance for prompt caching and Structured Outputs: <https://developers.openai.com/api/docs/guides/latest-model>
- OpenAI Structured Outputs: <https://developers.openai.com/api/docs/guides/structured-outputs>
- OpenAI Responses create reference: <https://developers.openai.com/api/reference/resources/responses/methods/create>
- LM Studio OpenAI compatibility: <https://lmstudio.ai/docs/developer/openai-compat>
- LM Studio Structured Output: <https://lmstudio.ai/docs/developer/openai-compat/structured-output>
- Hugging Face Inference Providers: <https://huggingface.co/docs/inference-providers/en/index>
- Hugging Face Responses API (beta): <https://huggingface.co/docs/inference-providers/en/guides/responses-api>
- Hugging Face Structured Outputs: <https://huggingface.co/docs/inference-providers/guides/structured-output>

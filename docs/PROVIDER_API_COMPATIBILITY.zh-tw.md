# 服務商 API 相容性

本文件記錄 NppAIAssistant 使用的傳輸契約。API client 負責服務商的
request／response 格式；提示詞模板只描述應顯示在 Notepad++ 內的助理答案。

## 支援基準

| 服務 | Base URL | 驗證 | 文字端點 | 回應文字位置 |
| --- | --- | --- | --- | --- |
| OpenAI | `https://api.openai.com/v1` | `Authorization: Bearer <token>` | `POST /chat/completions` | `choices[0].message.content` |
| LM Studio | 預設 `http://127.0.0.1:1234/v1` | 可選的本機 API token | `POST /chat/completions` 或 `POST /responses` | Chat content 或 Responses 的 `output_text` content |
| Hugging Face Inference Providers | `https://router.huggingface.co/v1` | `Authorization: Bearer <HF_TOKEN>` | `POST /chat/completions`；`/responses` 為 beta | Chat content 或 Responses 的 `output_text` content |

OpenAI-compatible 與 Copilot chat 路徑會讀取完整 JSON、尚未解析
Server-Sent Events，因此這些 request 都明確設定 `stream: false`。Gemini 與
Claude 使用各自 provider-specific 的 non-streaming endpoint，不使用
OpenAI-compatible 的 `stream` 欄位。通用 OpenAI 相容設定可選 Chat
Completions 或 Responses。Hugging Face model ID 可加上 `:fastest`、
`:cheapest`、`:preferred` 或指定 provider 的路由後綴。

通用相容設定使用的標準 Chat Completions request 為：

```json
{
  "model": "provider-model-id",
  "messages": [{ "role": "user", "content": "assembled prompt" }],
  "stream": false
}
```

client 從 `choices[0].message.content` 讀取助理文字，並保留有界的舊式
`choices[0].text` 相容 fallback。

標準 Responses request 為：

```json
{
  "model": "provider-model-id",
  "input": "assembled prompt",
  "store": false,
  "stream": false
}
```

client 會組合 `output` 中 `type` 為 `output_text` 的 content item。

## 提示詞與輸出格式的界線

預設 Output Contract 要求模型只回傳答案本身，不模擬 OpenAI、LM Studio
或 Hugging Face 的 API envelope。如果任務明確要求 JSON，則只回傳一個
有效 JSON value，不加 Markdown code fence。這是提示詞指引，不是 schema
強制驗證。

嚴格的 structured output 必須由 HTTP request 設定。OpenAI 與 LM Studio
的 Chat Completions 使用 `response_format.type = json_schema` 搭配 JSON
schema。schema JSON 不會複製進組裝後的提示詞。

## Structured Outputs（Phase A+B）

設定視窗的「提示詞與行為」分頁提供 `Text`、`Markdown`、`JSON` 與
`Structured JSON`。`JSON` 僅是提示詞指引；`Structured JSON` 同時使用
服務商原生 schema 傳輸與本機 JSON/schema 驗證界線。本階段刻意只提供有
大小上限的內建 preset（`Generic Structured Result`、`Document Review`）；
不包含可自由輸入的 schema editor。

| 服務／端點 | 能力 | Request 格式 | 證據狀態 |
| --- | --- | --- | --- |
| OpenAI Chat Completions | Native（僅限支援的 model） | `response_format.json_schema` | VERIFIED（官方格式）；實際 credential 測試為 NOT VERIFIED |
| OpenAI Responses | Native（僅限支援的 model） | `text.format` | VERIFIED（官方格式）；目前 OpenAI 設定 UI 尚未開放此 adapter path |
| LM Studio Chat Completions | Native transport（依 model 而定） | `response_format.json_schema` | VERIFIED（官方格式及 2026-08-22 有界本機 live test） |
| LM Studio Responses | Unknown／阻擋 | 不送 schema payload | NOT VERIFIED：LM Studio structured-output 文件僅涵蓋 Chat |
| 通用 OpenAI-compatible | Unknown／阻擋 | 不送 schema payload | INFERRED：generic profile 無法得知端點／model 能力 |
| Hugging Face aggregate router | Unknown／阻擋 | 不送 schema payload | INFERRED：下游 provider 與 model 可能變動 |
| Gemini、Claude、Copilot | Unsupported／阻擋 | 不送 schema payload | VERIFIED：本外掛目前的實作界線 |

Native Chat payload（schema 僅以 JSON object 出現一次，`strict` 為 JSON
boolean）：

```json
{
  "response_format": {
    "type": "json_schema",
    "json_schema": { "name": "structured_result", "strict": true, "schema": { } }
  }
}
```

Native Responses payload 使用不同的 wrapper：

```json
{
  "text": {
    "format": { "type": "json_schema", "name": "structured_result", "strict": true, "schema": { } }
  }
}
```

本機 validator 實作的是專案專用的封閉 subset，**不宣稱**符合完整 Draft
2020-12 或任何其他完整 JSON Schema draft。不支援的 keyword 在每一層 schema
node 都採 fail-closed。支援的 keyword 為 `type`、`properties`、`required`、
`additionalProperties`、`items`、`enum`、`const`、`minimum`、`maximum`、
`minLength`、`maxLength`。`type` 只能是一個支援的型別名稱，不支援 type
array／union；`additionalProperties` 只能是 boolean；`items` 只能是一個
schema，不支援 tuple form。JSON number 以十進位精確值比較；exponent 絕對值
上限為 1,000,000，超出即 fail closed。只有 transport、
已記錄的 content extraction、JSON parse，以及（啟用時）schema validation
全數成功後，回應才可使用。`finish_reason: "length"` 與 Responses 的
`status: "incomplete"`、`queued`、`in_progress`、`cancelled` 會回報為
generation incomplete，而非 JSON parse error；其他非 `stop` 的 Chat finish
reason 與 provider refusal 都會阻擋。矩陣中的 `Native` 意指端點 transport
shape 已有官方文件，不代表每個選定 model 都已 live-verified；若 provider
拒絕 schema，會回傳 typed error 且不做 fallback。驗證失敗時會保留已抽出的
model raw content 供檢閱，但不保留 request header 或 API key；失敗的
structured 回應不得取代編輯器選取文字。

OpenAI 官方列出的 Structured Outputs 支援範圍包括 `gpt-4o-mini`、
`gpt-4o-mini-2024-07-18`、`gpt-4o-2024-08-06` 與後續相容 model；較舊或
不支援的 model 可能拒絕 native request。LM Studio 也記錄了 model-dependent
的可靠性限制（尤其是較小 model），因此 plugin 不會把 provider 拒絕靜默
轉成 prompt-only fallback。

2026-08-22 的 loopback snapshot 使用 LM Studio 既已載入的
`openai/gpt-oss-20b` model 與 `http://127.0.0.1:1234/v1`，未附
Authorization header。valid schema、integer type enforcement、required
field、enum、prompt-only JSON 與 Text request 均回傳 HTTP 200 及預期輸出；
invalid schema 回傳 HTTP 400；`max_tokens: 1` 回傳
`finish_reason: "length"`。這只驗證該次本機 model/server snapshot，不代表
所有 LM Studio model 都具備相同 capability。

2026-08-23 的另一輪無 credential loopback smoke 確認同一 server 的
`GET /v1/models` 回傳 7 個 model。以 `openai/gpt-oss-20b` 測試時，具有足夠
completion budget 的 Chat Completions `stream: false` 回傳
`finish_reason: "stop"` 與 `local-ok`；native
`response_format.type: "json_schema"` 回傳含 required `result` property 的
JSON object。刻意使用極小的 8-token budget 時，回傳無 answer content 的
`finish_reason: "length"`。這些是 **VERIFIED** 的本機 snapshot 觀察，不是
throughput、token cost 或所有 model 的保證。

Loopback 模型探索使用 1.5 秒短 timeout，模型生成則使用獨立的五分鐘
timeout。本機推論不得沿用探索 timeout；較大的 CPU／GPU offload model 產生
完整 non-streaming 回應，合理上可能需要超過 30 秒。

## 官方參考資料

- OpenAI Chat Completions API：<https://developers.openai.com/api/reference/resources/chat/subresources/completions/methods/create>
- OpenAI 提示詞快取與 Structured Outputs 指引：<https://developers.openai.com/api/docs/guides/latest-model>
- OpenAI Structured Outputs：<https://developers.openai.com/api/docs/guides/structured-outputs>
- OpenAI Responses create reference：<https://developers.openai.com/api/reference/resources/responses/methods/create>
- LM Studio OpenAI 相容介面：<https://lmstudio.ai/docs/developer/openai-compat>
- LM Studio Structured Output：<https://lmstudio.ai/docs/developer/openai-compat/structured-output>
- Hugging Face Inference Providers：<https://huggingface.co/docs/inference-providers/en/index>
- Hugging Face Responses API（beta）：<https://huggingface.co/docs/inference-providers/en/guides/responses-api>
- Hugging Face Structured Outputs：<https://huggingface.co/docs/inference-providers/guides/structured-output>

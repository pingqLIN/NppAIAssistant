# 提示詞政策與執行期政策

## 狀態

- **VERIFIED：** NppAIAssistant 是單輪的 editor assistant，沒有讓模型呼叫
  檔案、shell、網路、MCP 或 workflow 工具的介面。
- **VERIFIED：** provider transport、Structured Output capability、JSON parse、
  schema validation、回應診斷與 editor 回寫均由 C++ runtime 執行；使用者編輯
  Task Instructions 無法移除這些行為。
- **VERIFIED：** 選取內容與可選本機 Memory 在組裝後都標記為不可信資料。
- **NOT VERIFIED：** 單靠提示詞文字不足以防止所有 prompt injection；本版本
  沒有通用工具執行面可供模型濫用。

## 邊界

| 層次 | 責任 | 強制方式 |
| --- | --- | --- |
| Runtime Policy | Provider request、timeout、結構化驗證、editor 回寫、脫敏 | C++ runtime |
| Prompt Policy | 單輪揭露、證據限制、誠實性、不可信資料處理 | 組裝的 prompt；必須明確解除鎖定後才能編輯 |
| Task Profile | General、Review、Transform、Code Change、Documentation | prompt assembly |
| Request Data | Task Instructions、選取內容、可見本機 Memory | 使用者資料，不是 policy |

`Prompt Policy` 刻意保持精簡：只使用可見資料、說明重要未知事項、不假稱已做
不可用的動作、保留來源資料邊界、遵循 runtime 的輸出合約，並對 editor
transformation 優先採取最小正確修改。

## 提示詞區塊鎖定

內建的 Prompt Policy、Identity、Rules、Assignment、Scenario Modules 與
Output Contract 預設皆為**鎖定**。在 Prompt Sections 視窗中取消勾選
**鎖定區塊**後才可編輯；內容與鎖定狀態僅會在外層 Settings 按下 **OK** 時儲存。
Reset 會還原該區塊的產生／內建預設內容，但不會改變鎖定狀態。

Runtime Context、目前 Task Instruction、選取的 Source Context 與可見 Memory
仍是 request data 或 runtime snapshot，不是可編輯的 template。解除提示詞區塊鎖定
不會授予工具權限、不會略過 provider capability check、不會移除 Structured JSON 的
transport/schema validation，也不會改變 editor 回寫保護。

## Composer 行為

Composer 只讓使用者編輯 **Task Instructions**，並顯示唯讀 runtime 摘要。按下
Send 時，plugin 重新組裝選定的 policy、Output Contract、runtime context、task
profile、可選 Memory、使用者任務和 source data。最終 wire prompt 不再被當成
可編輯的安全控制項。

Structured JSON 仍是 transport layer 的資料契約。schema 不會插入 Task
Instructions，回應擷取後仍會執行 client-side validation。

## 面板控制與請求可視性

助理面板直接顯示並可選 Task Profile、Output Mode、送出前預覽偏好與兩種受限制的 prompt 組合：

- `Runtime -> Policy -> Profile -> Memory -> Task -> Source (cache-first)`
  是預設值。其穩定的 policy/profile 前綴在支援相容 prompt caching 的 provider 上「可能」可重複使用。
  此為 **INFERRED**，不是 provider cache 保證。
- `Runtime -> Policy -> Profile -> Task -> Memory -> Source` 是可選的請求
  資料排列；不會把 task、memory 或 source data 移到受治理的政策／profile 前面。

兩種排列都將 Runtime Policy、Prompt Policy、Task Profile 與 Output Contract
置於所有 request data 之前。
Runtime 行為與 Structured JSON schema 不是可移動的 prompt section。面板的狀態列會分別顯示
本機組裝、HTTP transport 連線、傳送、等待供應商回應、接收、本機 parse/validation、成功、
失敗與預覽取消。它只保留狀態標籤，不保留 prompt、endpoint URL、header、credential 或 model output。

送出前預覽預設開啟。關閉後仍使用同一個 prompt assembler 與 provider-neutral
Structured Output config，只是跳過可編輯的 Composer dialog。啟用預覽時按 Cancel 不會開始請求。

`New` 與 `Branch` 是本機 transcript 操作。Branch 會複製可見的面板訊息供查看，但不會自動將任何舊訊息傳給 provider，
以保持已驗證的單輪契約。

## 來源資料與診斷

選取的文件內容會以以下形式送出：

```xml
<source_context trust="untrusted">
…選取的文件內容…
</source_context>
```

可見 Memory 使用相同邊界。明確的 task 可以要求模型分析或採納來源中的指令，
但來源資料本身不能取代 Prompt Policy。

`PromptAssemblyManifest` 是 UI／診斷摘要的記憶體中繼資料，只含 policy version、
task profile、output mode、validation state、section ID、included state、trust、
字元數與 token estimate；不保留 prompt、selection、Memory、provider response 或
API key payload。

## Future Work

Tool execution、autonomous retry、long-term memory、multi-agent handoff、MCP 與
AGENTS.md loader 都不屬於本次範圍。日後若引入實際 tool surface，必須先提供
capability allowlist、執行前核准、tool input/output validation 與可稽核的執行紀錄。

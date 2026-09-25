# 使用說明

## 適用情境

`NppAIAssistant` 是在 Notepad++ 內協助處理單一、明確任務的輕量 AI 外掛。

適合用於：

- 說明選取的程式碼或文字
- 重構或修正選取區塊
- 為程式碼補上註解
- 提出一次性的實作問題
- 在 AI 面板完成簡短問答

它不是長期記憶的聊天工作區；每次請求都是獨立處理的單輪互動。

## 安裝到 Notepad++

### 手動安裝

1. 建置外掛 DLL。
2. 若尚未存在，建立資料夾：`<Notepad++>\plugins\NppAIAssistant\`。
3. 將 `NppAIAssistant.dll` 複製到該資料夾。
4. 重新啟動 Notepad++。

### 輔助腳本

也可使用 `scripts/install-npp-ai-plugin.ps1`。腳本預設假設 Notepad++
安裝在 `C:\Program Files\Notepad++`，且已建置的 DLL 位於本專案的 `build/`
輸出目錄。

## 第一次設定

1. 開啟外掛設定。
2. 輸入 OpenAI、Gemini 或 Claude 的 API Key。
3. 選擇預設 provider。
4. 按下 `Test Default Connection`。
5. 確認外掛能為目前 provider 動態載入模型。
6. 視需要選擇介面語言。

## 設定與機敏資料儲存

- API Key 與 OAuth token 儲存在 `%LocalAppData%\Notepad++\AIAssistant`。
- 機敏值使用 Windows DPAPI 保護。
- 非機敏偏好設定儲存在 `%AppData%\Notepad++\plugins\config\NppAIAssistant.ini`。
- 舊版 roaming secure blob 會在新版首次啟動時自動遷移。

## 提示詞組裝

設定視窗提供單輪提示詞組裝器，可調整：

- preset
- 回覆語言
- 編碼偏好
- 回覆細節
- 情境模組
- 輸出規則
- 透過 `Prompt Sections...` 檢視及調整內建 policy 與模板區塊

預覽會隨設定即時更新，並顯示下列可見區塊：

- Prompt Policy
- Identity
- Rules
- Memory
- Runtime Context
- Assignment
- Scenario Modules
- Output Contract
- Task Instruction
- Source Context

不應把提示詞文字視為唯一的安全邊界。provider capability 判定、Structured JSON
request transport、本機 JSON/schema 驗證、診斷資料脫敏，以及 editor 回寫保護，
都由程式的 runtime path 強制處理。

預覽也會顯示各區塊和總計的 token 估算值。這是本機估算，不等同 provider 帳單或
context-window 的精確 token 數。

在 `Prompt Sections...` 可檢視 Prompt Policy、Identity、Rules、Assignment、
Scenario Modules 與 Output Contract。這些內建區塊預設鎖定；要編輯或重設時，
請先取消勾選 `鎖定區塊`。Token 下拉選單可將 `{{PROVIDER}}`、`{{MODEL}}`、
`{{LANGUAGE}}`、`{{TIMESTAMP}}` 等變數插入目前的模板編輯器。

每個提示詞區塊模板上限為 30,000 個字元，確保可以安全地寫入及讀回本機設定檔。
重設會回復內建預設值，但不會改變鎖定狀態。此對話框按 `OK` 只會更新外層 Settings
的暫存內容；外層 Settings 也按 `OK` 才會真正儲存。

## Preset

目前的 preset 適合快速、一次性的任務：

- Custom
- Code Fix
- Refactor
- Explain Code
- Generate Tests
- Write Docs

這些 preset 不會加入隱藏記憶，只會調整當次請求的提示策略。

## Memory 儲存

Memory 預設關閉。可在設定中的 `Memory...` 啟用可見的 Memory 區塊，並編輯會隨請求
送出的記憶文字。預覽會顯示它是否關閉，或估計增加多少 token。

Memory 以一般外掛設定明文儲存，不使用 DPAPI 機敏儲存。請勿放入 API Key、OAuth token、
密碼、客戶私密資料，或任何不應出現在一般 Notepad++ 外掛設定檔的內容。

## 功能邊界

目前版本尚未提供：

- OpenAI、Gemini、Claude 或代管 broker 的正式 OAuth 登入
- 浮動式 inline editor overlay

既有暫停中的 Copilot OAuth 程式碼，不代表正式完成 OAuth 登入功能。相關工作必須分別
說明為儲存模型、provider UI 與真實 provider 登入驗證的進度。

## 請求狀態

Provider 請求在 UI 執行緒外處理。請求進行時，面板使用獨立狀態列顯示準備、連線、送出、
等待、收到回應、解析／驗證、完成或失敗。provider、模型與送出控制項會視狀態停用。
歷史對話文字不會為了等待動畫而重繪；請求完成後，結果才會回到 UI 執行緒更新對話或選取文字。

## 自訂內容模板

在設定的 `Context Templates...` 可設定最多三個右鍵模板。每個欄位包含：

- 啟用開關
- 選單名稱
- 提示詞模板
- 是否取代選取內容

啟用後，當 Notepad++ 有選取文字時，模板會出現在標準 AI 右鍵動作中。若選擇取代模式，
非同步請求完成後，只有原始 buffer 仍為目前作用中的文件時才會寫回結果。

## 提示詞預覽

`Prompt Preview` 可檢視當次請求會使用的實際提示詞結構，協助確認：

- 回覆語言是否正確
- 輸出是否應簡短或詳細
- 是否針對修正、重構、測試或文件調整
- 外掛實際送出的內容是否符合預期

## 單輪行為

本外掛刻意採單輪設計：

- 不保留長期隱藏記憶
- 每次請求各自獨立
- 提示詞較容易檢視
- 較容易理解回應產生的原因

## 右鍵選單動作

在 Notepad++ 選取文字後，可使用下列 AI 右鍵動作：

- AI: Explain Selection
- AI: Refactor Selection
- AI: Add Comments
- AI: Fix Selection

這些動作針對快速的編輯器內操作設計。

## AI 面板流程

1. 開啟 AI 面板。
2. 選擇 provider 與模型。
3. 輸入任務。
4. 送出。
5. 檢視格式化的回應。

若啟用 `Ctrl+Enter` 模式，單按 Enter 不會直接送出。面板工具列的 `A+` 和 `A-`
可調整對話與輸入文字大小；比例會以非機敏偏好設定保存，並限制在 80% 到 150% 之間。

## 模型載入

包含 LM Studio 在內的本機 loopback 服務，Chat Completions 與 Responses
生成請求使用 900 秒（15 分鐘）逾時。模型探索仍為 1.5 秒，遠端服務保留原有逾時。
生成逾時套用於 WinHTTP 傳輸階段，不代表整個請求保證在 15 分鐘內結束。
錯誤碼 12002 表示傳輸逾時。

設定完成且 provider 可使用時，外掛會動態載入模型，避免使用容易過期的硬編碼清單，並更貼近
目前 provider 帳號或本機服務實際提供的模型。

## 建議的 GitHub 展示流程

若要準備截圖或短展示，可依序呈現：

1. 設定視窗與提示詞預覽。
2. 切換 preset。
3. 右鍵 AI 動作。
4. AI 面板中的單次請求。
5. 強調它是輕量、單輪的編輯器輔助工具。

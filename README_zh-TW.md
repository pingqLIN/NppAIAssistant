# NppAIAssistant

NppAIAssistant 是在 Windows 上運行的 Notepad++ AI 外掛，讓使用者在編輯程式或文字時，
以清楚可見、單次獨立的方式取得 AI 協助。提示詞的組成可以在設定中檢視；每次請求不會暗中
沿用先前對話內容。

[English](README.md)

## 從這裡開始

本專案是可獨立建置的 C++20 Notepad++ 外掛。可先用 CMake 產生 x64 DLL：

```powershell
cmake -S . -B build-cmake
cmake --build build-cmake --config Release
```

將產生的 `NppAIAssistant.dll` 複製到：

```text
<Notepad++>\plugins\NppAIAssistant\NppAIAssistant.dll
```

重新啟動 Notepad++ 後，開啟 **Plugins > NppAIAssistant > Settings**，設定供應商、
測試連線，再從取得的模型清單中明確選擇一個模型，便可以送出請求。完整流程請參閱
[使用說明](docs/USAGE.md)。

## 可以做什麼

- 以停駐式 AI 面板提出一次性的問題並查看回覆。
- 透過預設方案、回覆語言與編碼建議、詳細程度、情境模組、輸出規則，以及提示詞預覽，
  調整單次請求。
- 編輯 Identity、Rules、Assignment 區塊；也可以啟用可見的 Memory。Memory 預設關閉，
  不會成為隱藏上下文。
- 向 OpenAI、Gemini、Claude 或選用的本機 OpenAI 相容 `/v1` 服務取得模型清單。
- 已保存的模型若不再存在，外掛不會自行改選其他模型；必須由使用者重新選取後才能送出。
- 在選取文字後執行解說、重構、加註解或修正。AI 右鍵選單只會在「選取非空文字後按住 Ctrl
  再用滑鼠按右鍵」時出現；一般右鍵與鍵盤內容選單仍由 Notepad++ 原生處理。
- 介面提供英文與繁體中文。

## 隱私與連線界線

API Key 會以 Windows DPAPI 保護並存放在本機應用程式資料夾。提示詞模板、已選模型、逾時秒數
與選用的本機端點屬於非機敏偏好，會存放在 `%AppData%` 下的 Notepad++ 外掛設定檔。

本機 OpenAI 相容供應商僅接受字面上的 loopback `/v1` 端點：
`http://127.0.0.1:<port>/v1` 或 `http://[::1]:<port>/v1`。輸入 `localhost` 時會轉為
`127.0.0.1`；遠端主機不會通過驗證。本機請求會略過 Proxy、拒絕重新導向，也不接受遠端位址。

請求逾時預設為 30 秒，可在設定中調整為 1 到 300 秒。每個請求開始前都會保留自己的逾時、
端點、模型與憑證快照，因此設定變更不會影響已經執行中的請求。

請勿將密碼、客戶資料、API Key 或其他機敏資訊寫入提示詞模板或選用的 Memory 欄位；這些內容
屬於一般外掛偏好設定，並非秘密儲存空間。

## 文件

- [使用說明](docs/USAGE.md) — 安裝、首次設定、提示詞工具、右鍵行為與排除方式。
- [本機供應商與請求逾時](docs/LOCAL_PROVIDER_AND_TIMEOUT.zh-tw.md) — loopback 端點政策、
  模型選取原則與手動驗收項目。
- [專案更新紀錄](docs/CHANGELOG.zh-tw.md) / [Project change log](docs/CHANGELOG.md)
  — 目前尚未發行變更的使用者導向摘要。
- [Plugins Admin 提交指南](docs/PLUGIN_ADMIN_SUBMISSION.md) — 打包與官方清單的前置條件；
  這不代表外掛已上架。
- [專案結構](PROJECT_STRUCTURE.md) — 原始碼與打包檔案的位置。

## 驗證與發佈狀態

專案提供安全性回歸檢查與打包就緒工具：

```powershell
.\scripts\verify-security-regressions.ps1
.\scripts\package-npp-ai-plugin.ps1 -Platform x64
.\scripts\smoke-package-install.ps1
.\scripts\verify-release-readiness.ps1 -Platform x64 -Configuration Release
```

程式建置與靜態檢查不能取代 Notepad++ 的實機驗收。公開發佈前，仍應從 ZIP 套件安裝測試，
確認設定視窗、右鍵手勢、本機端點拒絕、模型選取與卸載行為。

NppAIAssistant 已有對應 0.1.0.0 版本的 x64 Plugins Admin 項目；本分支的原始碼變更並不是該
已發佈套件的更新。未來若要更新，仍須先提高 DLL 版本、建立 GitHub Release 的直接 HTTPS ZIP
連結、取得該檔案的最終 SHA-256、更新對應架構的 JSON 項目，並經過上游維護者審查。

## 參與與授權

歡迎透過 [pingqLIN/NppAIAssistant](https://github.com/pingqLIN/NppAIAssistant)
提交 issue 或 pull request。請勿將 API Key、機敏資料、個人設定或建置產物提交到專案中。

本專案採用 [GNU GPL version 3](LICENSE) 授權。

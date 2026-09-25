# 原生面板預覽

此獨立 CMake target 編譯正式面板程式與資源，在 Notepad++ 就緒時自動顯示面板。
預覽版使用記憶體設定、停用金鑰儲存，並以拒絕請求的 adapter 取代 HTTP。
它供 UI 試看，不是 AI 服務測試或發布驗收版本，禁止將此 DLL 當作正式版本發布。

以本目錄為 CMake source，在獨立 build 目錄建置並執行 CTest。
使用全新的 portable Notepad++ 目錄，僅放入此預覽外掛；不要複製既有使用者設定、
session 或整個 plugins 目錄。正式 repository 建置與發布套件不受影響。

## 可連線測試版

使用獨立 build 目錄並設定 `-DNPPAI_PREVIEW_ENABLE_NETWORK=ON`，即可使用正式 HTTP
程式碼測試 LM Studio 或 OpenAI 相容服务。標題會顯示「連線測試」。設定僅留在記憶體；
使用者於設定視窗輸入的金鑰以 DPAPI 加密後暫存於此程序，關閉後不保留，亦不讀取既有
credential 路徑。此版本仍非正式發布 DLL。

LM Studio 預設 Base URL 是 `http://127.0.0.1:1234/v1`。在設定中探索模型、明確選擇模型
並按確定。外部相容服務使用該服務的 Base URL 與 API key；不要把完整的
`/chat/completions` 請求端點填入 Base URL。

# NppAIAssistant 0.2.0.6（Windows x64）

Notepad++ 內的 AI 工作台，提供可見提示詞、本機 LM Studio 與 OpenAI 相容服務、
基本 Markdown／JSON 顯示，以及編輯器輸出。

安裝前先關閉 Notepad++，將 NppAIAssistant.dll 放到 x64 Notepad++ 目錄下的
`plugins/NppAIAssistant/NppAIAssistant.dll`。保留舊 DLL 供回復，重新啟動後開啟工作台。
送出前請檢視選定的服務、模型與提示詞。

本地生成使用 900 秒的 WinHTTP 階段逾時，模型探索使用 1.5 秒；這不代表整個請求的
總時間上限。遠端服務限制不變。模型／服務必須可用，雲端服務可能收費。

請求會把可見提示詞及選定內容送至所選服務。API 金鑰透過 Windows DPAPI 保護，
存於目前使用者的 LocalAppData；偏好設定與選用記憶以明文存於 AppData。
可攜式 Notepad++ 不會隔離這些儲存位置，請勿把機密放進提示詞記憶。

本候選包僅針對 x64，尚未認證其他架構及最低 Notepad++ 相容版本。
日文與西班牙文涵蓋主要工作台，進階設定部分使用英文。

操作及限制請見 USAGE.zh-tw.md、WORKBENCH.zh-tw.md、EDITOR_OUTPUT.zh-tw.md。
原始碼與問題回報：https://github.com/pingqLIN/NppAIAssistant
授權：GPL-3.0-or-later，詳見 LICENSE。

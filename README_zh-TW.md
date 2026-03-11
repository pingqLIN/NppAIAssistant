[English](README.md)

# NppAIAssistant for Notepad++

一個為 Notepad++ 設計的輕量 AI 外掛，強調提示詞可視性、模組化單輪提示設定，以及不保留隱藏記憶的可預測行為。

這個 repository 專注在外掛本身，不攜帶完整 Notepad++ 上游歷史，因此更適合公開發布、程式審查、打包與版本管理。

## 專案亮點

- 採用輕量外掛架構，而不是長期維護核心 fork
- 提示詞可視化，可直接預覽實際送出的提示結構
- 單輪對話設計，不依賴跨請求隱藏記憶
- 登入或設定 API Key 後可動態取得模型
- 右鍵功能直接融入編輯流程
- 介面支援英文與繁體中文

## 核心體驗

### 輕量化設計
`NppAIAssistant` 以一般 Notepad++ 外掛方式發佈。安裝方式熟悉、維護負擔較低，也更容易跟隨 Notepad++ 外掛生態進行打包與釋出。

### 提示詞可視性
設定視窗提供提示詞預覽區。當你切換 preset、回覆語言、編碼建議、詳略程度、情境模組或輸出規則時，提示詞預覽會即時更新。

### 不保留隱藏記憶
每次請求都被視為獨立的單輪互動。外掛不依賴聊天歷史作為隱藏上下文，因此行為更容易理解、檢查與稽核。

### 以編輯工作流為中心
除了 AI 面板外，也可以直接在編輯器右鍵選單觸發解說、重構、加註解、修正等常用操作。

## 截圖

### 設定中的提示詞預覽
![Prompt Preview](docs/assets/screenshots/settings-prompt-preview.png)

### Preset 驅動的提示組裝
![Preset Dropdown](docs/assets/screenshots/settings-preset-dropdown.png)

### 右鍵功能選單
![Context Menu Actions](docs/assets/screenshots/context-menu-actions.png)

## 專案結構

- `src/` 外掛主程式、資源檔、版本資訊
- `src/shared/` HTTP、Provider API、安全儲存相關共用元件
- `vendor/notepadpp/` 為獨立建置所需的 Notepad++ plugin 與 docking headers
- `vendor/scintilla/include/` 外掛介面所需的 Scintilla headers
- `docs/` 使用說明、發佈說明、提交 Plugins Admin 文件
- `scripts/` 安裝與打包輔助腳本

延伸閱讀：

- [專案結構](PROJECT_STRUCTURE.md)
- [使用說明](docs/USAGE.md)
- [Plugins Admin 提交指南](docs/PLUGIN_ADMIN_SUBMISSION.md)

## 建置

### Visual Studio / MSBuild

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" "NppAIAssistant.vcxproj" /p:Configuration=Release /p:Platform=x64 /m
```

### CMake

```powershell
cmake -S . -B build
cmake --build build --config Release
```

預期輸出：

`build/x64/Release/plugins/NppAIAssistant/NppAIAssistant.dll`

## 安裝

將編譯出的 DLL 複製到：

`<Notepad++>\plugins\NppAIAssistant\NppAIAssistant.dll`

或使用：

`scripts/install-npp-ai-plugin.ps1`

## 發佈與 Plugins Admin

此 repo 已包含對應 Notepad++ Plugins Admin 風格發佈所需的打包與提交輔助檔案。

- 打包腳本：`scripts/package-npp-ai-plugin.ps1`
- Metadata：`plugin-admin-metadata.json`
- 提交說明：[docs/PLUGIN_ADMIN_SUBMISSION.md](docs/PLUGIN_ADMIN_SUBMISSION.md)

建議版本配置：

- GitHub release tag：`v0.1.0`
- 外掛版本：`0.1.0.0`
- Release 資產：`NppAIAssistant-0.1.0.0-x64.zip`

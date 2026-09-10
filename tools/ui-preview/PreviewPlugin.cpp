// Build the real panel with isolated adapters and automatic preview startup.
// This translation unit is only part of the separate tools/ui-preview target.
#define beNotified productionBeNotified
#define getName productionGetName
#include "../../src/NppAIAssistant.cpp"
#undef beNotified
#undef getName

extern "C" __declspec(dllexport) const wchar_t *getName() {
#ifdef NPPAI_PREVIEW_ENABLE_NETWORK
  return L"NppAIAssistant Workspace - Connected Test";
#else
  return L"NppAIAssistant Workspace - UI Preview";
#endif
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *notification) {
  productionBeNotified(notification);
  if (notification && notification->nmhdr.code == NPPN_READY) {
    showPanel();
#ifdef NPPAI_PREVIEW_ENABLE_NETWORK
    ::SetWindowTextW(::GetDlgItem(g_panel, IDC_AI_PANEL_TITLE), L"NppAIAssistant 工作台");
    addMessage(false,
        L"可連線測試版：LM Studio 與 OpenAI 相容服務使用正式 HTTP 程式碼。\r\n\r\n"
        L"請在設定中探索模型、明確選擇預設模型，再按確定。\r\n"
        L"LM Studio 預設網址：http://127.0.0.1:1234/v1\r\n\r\n"
        L"送出會連線至你選擇的服務。設定與金鑰只保留在此次執行中，關閉後需重新輸入。");
#else
    ::SetWindowTextW(::GetDlgItem(g_panel, IDC_AI_PANEL_TITLE), L"NppAIAssistant 工作台");
    addMessage(false,
        L"這是新版面板的原生 UI 預覽。\r\n\r\n"
        L"可以調整側欄寬度、拖曳輸入區分隔線、切換輸出模式，以及開啟設定。\r\n\r\n"
        L"設定僅保留在此次執行的記憶體中；AI 網路請求與金鑰儲存已停用。");
#endif
    updateChatDisplay();
  }
}

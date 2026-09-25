// Exercise production writing functions against deterministic Win32 message peers.
// No provider calls, visible UI, installed editor or user documents are involved.
#include "../../src/NppAIAssistant.cpp"
#include <iostream>
#include <cstdlib>

namespace {
std::string document = "abc";
std::string undoDocument;
int encoding = 65001, beginCount = 0, endCount = 0;
bool readOnly = false, rejectNew = false;
UINT_PTR buffer = 1;
Sci_Position targetStart = 0, targetEnd = 0, caret = 1, selStart = 0, selEnd = 2;
LRESULT CALLBACK peer(HWND window, UINT message, WPARAM wp, LPARAM lp) {
  switch (message) {
  case NPPM_GETCURRENTSCINTILLA: *reinterpret_cast<int *>(lp) = 0; return TRUE;
  case NPPM_GETCURRENTDOCINDEX: return 0;
  case NPPM_GETBUFFERIDFROMPOS:
  case NPPM_GETCURRENTBUFFERID: return buffer;
  case NPPM_MENUCOMMAND:
    if (lp == 41001 && !rejectNew) { ++buffer; document.clear(); caret = 0; }
    return TRUE;
  case SCI_GETLENGTH: return document.size();
  case SCI_GETDOCPOINTER: return buffer + 100;
  case SCI_GETCODEPAGE: return encoding;
  case SCI_GETTEXT:
    memcpy(reinterpret_cast<void *>(lp), document.c_str(), document.size() + 1);
    return document.size();
  case SCI_GETREADONLY: return readOnly;
  case SCI_GETCURRENTPOS: return caret;
  case SCI_GETSELECTIONSTART: return selStart;
  case SCI_GETSELECTIONEND: return selEnd;
  case SCI_GETSELECTIONS: return 1;
  case SCI_SELECTIONISRECTANGLE: return FALSE;
  case SCI_SETTARGETSTART: targetStart = wp; return 0;
  case SCI_SETTARGETEND: targetEnd = wp; return 0;
  case SCI_BEGINUNDOACTION: ++beginCount; undoDocument = document; return 0;
  case SCI_ENDUNDOACTION: ++endCount; return 0;
  case SCI_UNDO: document = undoDocument; return 0;
  case SCI_REPLACETARGET:
    document.replace(targetStart, targetEnd - targetStart,
                     reinterpret_cast<const char *>(lp), wp);
    return wp;
  }
  return ::DefWindowProcW(window, message, wp, lp);
}
void check(bool value, const char *label) {
  if (!value) { std::cerr << label << '\n'; std::exit(1); }
}
}
int main() {
  WNDCLASSW cls{};
  cls.lpfnWndProc = peer; cls.hInstance = ::GetModuleHandleW(nullptr);
  cls.lpszClassName = L"NppEditorWriteTestPeer";
  ::RegisterClassW(&cls);
  HWND window = ::CreateWindowW(cls.lpszClassName, L"", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, cls.hInstance, nullptr);
  check(window != nullptr, "message peer creation");
  g_nppData._nppHandle = window; g_nppData._scintillaMainHandle = window;
  auto target = captureEditorWriteTarget(false);
  check(writeEditorOutput(target, L"中文"), "UTF8 insertion");
  check(document == "a\xe4\xb8\xad\xe6\x96\x87" "bc", "insert at saved cursor");
  check(beginCount == 1 && endCount == 1, "single undo group");
  ::SendMessageW(window, SCI_UNDO, 0, 0);
  check(document == "abc", "undo restores original");
  target = captureEditorWriteTarget(true);
  check(writeEditorOutput(target, L"X") && document == "Xc", "selection replacement");
  document = "abc"; target = captureEditorWriteTarget(false); document = "abC";
  check(!writeEditorOutput(target, L"X") && document == "abC", "changed document preserved");
  document = "abc"; target = captureEditorWriteTarget(false); ++buffer;
  check(!writeEditorOutput(target, L"X"), "switched document blocked");
  target = captureEditorWriteTarget(false); readOnly = true;
  check(!writeEditorOutput(target, L"X"), "read only blocked"); readOnly = false;
  encoding = 1252; target = captureEditorWriteTarget(false);
  check(!writeEditorOutput(target, L"中文") && document == "abc", "lossy encoding blocked");
  encoding = 65001; target = captureEditorWriteTarget(false);
  check(!writeEditorOutput(target, std::wstring(L"a\0b", 3)), "embedded null blocked");
  rejectNew = true;
  check(!writeNewOutputDocument(L"X") && document == "abc", "failed new preserves active file");
  rejectNew = false;
  check(writeNewOutputDocument(L"new") && document == "new", "new document output");
  check(requestProgressForHttpPhase(HttpTransportPhase::Failed) == RequestProgressState::Failed,
        "HTTP failure is not reported as disconnection");
  check(requestProgressForHttpPhase(HttpTransportPhase::ConnectionFailed) == RequestProgressState::Disconnected,
        "transport connection failure has a distinct state");
  for (int language = 1; language <= 4; ++language) {
    check(static_cast<int>(sanitizeUiLanguagePreference(language)) == language, "UI language roundtrip");
    check(static_cast<int>(sanitizePromptResponseLanguage(language)) == language, "response language roundtrip");
  }
  AIAssistantConfig config;
  PromptAssemblyContext context;
  for (auto language : {UiLanguage::Japanese, UiLanguage::Spanish}) {
    g_uiLanguage = language; context.uiLanguage = language;
    check(getPromptResponseLanguageInstruction(config, context) ==
        (language == UiLanguage::Japanese ? L"Japanese" : L"Spanish"), "follow-interface response language");
    commandMenuInit();
    check(std::wstring(g_funcItems[0]._itemName) != L"Open workspace", "localized menu");
    check(g_funcItems[1]._pFunc == nullptr && g_funcItems[6]._pFunc == nullptr &&
          g_funcItems[11]._pFunc == nullptr, "menu group separators");
  }
  config.outputMode = OutputMode::Markdown; config.outputMentionRisks = true;
  const auto replacement = buildPromptAssemblyResult(config, context, L"Fix this", true, L"x");
  check(replacement.fullPrompt.find(L"Briefly call out important risks") == std::wstring::npos,
        "replacement output excludes conflicting risk prose");
  check(replacement.fullPrompt.find(L"Format the answer as readable Markdown") == std::wstring::npos,
        "replacement output excludes conflicting Markdown formatting");
  applyPromptPresetToConfig(config, PromptPreset::Review);
  const auto review = buildPromptAssemblyResult(config, context, L"Review", false, L"x");
  check(review.fullPrompt.find(L"Prioritize concrete findings") != std::wstring::npos, "review preset has concrete criteria");
  // Use the actual system Rich Edit control to validate offsets and formatting.
  check(::LoadLibraryExW(L"Msftedit.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32) != nullptr, "Rich Edit load");
  HWND rich = ::CreateWindowExW(0, L"RICHEDIT50W", L"", WS_CHILD | ES_MULTILINE | ES_READONLY,
      0, 0, 400, 400, window, reinterpret_cast<HMENU>(IDC_AI_CHAT_HISTORY), cls.hInstance, nullptr);
  check(rich != nullptr, "Rich Edit creation");
  g_panel = window; g_chatHistory.clear();
  addMessage(false, L"# Title\n**bold** and `code`\n");
  updateChatDisplay();
  auto visible = getControlText(rich);
  check(visible.find(L"Title") != std::wstring::npos && visible.find(L"**bold**") == std::wstring::npos,
        "Markdown rendering on real Rich Edit");
  std::wstring internal(visible.size() + 1, L'\0');
  GETTEXTEX getText{}; getText.cb = static_cast<DWORD>(internal.size() * sizeof(wchar_t));
  getText.flags = GT_DEFAULT; getText.codepage = 1200;
  const auto internalLength = ::SendMessageW(rich, EM_GETTEXTEX, reinterpret_cast<WPARAM>(&getText), reinterpret_cast<LPARAM>(internal.data()));
  internal.resize(static_cast<size_t>(internalLength));
  const auto bold = internal.find(L"bold");
  CHARRANGE range{static_cast<LONG>(bold), static_cast<LONG>(bold + 4)};
  ::SendMessageW(rich, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
  CHARFORMAT2W format{}; format.cbSize = sizeof(format);
  ::SendMessageW(rich, EM_GETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
  check((format.dwEffects & CFE_BOLD) != 0, "bold range offsets");
  g_formatPreview = false; updateChatDisplay();
  check(getControlText(rich).find(L"**bold**") != std::wstring::npos, "raw reply preserved");
  check(document == "new", "display formatting does not change editor content");
  g_panelUiFont = ::CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
      DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
  check(g_panelUiFont != nullptr, "panel font creation");
  const HFONT panelFont = g_panelUiFont;
  PromptSectionsDlgProc(window, WM_NCDESTROY, 0, 0);
  check(g_panelUiFont == panelFont && ::GetObjectType(panelFont) == OBJ_FONT,
        "closing prompt sections must preserve panel fonts");
  PanelDlgProc(window, WM_NCDESTROY, 0, 0);
  check(g_panelUiFont == nullptr && g_chatFont == nullptr && g_panelTitleFont == nullptr,
        "panel destruction clears owned font handles");
  g_panel = nullptr;
  ::DestroyWindow(window);
  std::cout << "Production editor write integration scenarios passed\n";
}

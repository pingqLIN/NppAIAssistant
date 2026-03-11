#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>
#include <cwctype>
#include <sstream>
#include <string>
#include <vector>

#include "PluginInterface.h"
#include "Docking.h"
#include "dockingResource.h"
#include "LLMApiClient.h"
#include "SecureStorage.h"
#include "NppAIAssistantResources.h"

namespace {
constexpr wchar_t kPluginName[] = L"NppAIAssistant";
constexpr wchar_t kPanelTitle[] = L"AI Assistant";
constexpr size_t kMenuCount = 6;
constexpr UINT_PTR kCopilotPollTimerId = 9001;
constexpr DWORD kDefaultCopilotPollMs = 5000;
constexpr int kDefaultFontSize = 10;
constexpr int kMinFontSize = 8;
constexpr int kMaxFontSize = 18;
constexpr UINT kAiContextExplain = 1;
constexpr UINT kAiContextRefactor = 2;
constexpr UINT kAiContextComments = 3;
constexpr UINT kAiContextFix = 4;

enum class LLMProvider { OpenAI = 0, Gemini, Claude, Copilot, ProviderCount };
constexpr std::array<LLMProvider, 3> kEnabledProviders = {
    LLMProvider::OpenAI, LLMProvider::Gemini, LLMProvider::Claude};

enum class UiLanguage {
  English,
  Chinese,
};

enum class UiLanguagePreference {
  FollowNotepad = 0,
  English = 1,
  Chinese = 2,
};

enum class PromptResponseLanguage {
  FollowInterface = 0,
  TraditionalChinese = 1,
  English = 2,
};

enum class PromptEncodingPreference {
  CurrentDocument = 0,
  UTF8 = 1,
  UTF8Bom = 2,
  Big5 = 3,
  ANSI = 4,
};

enum class PromptPreset {
  Manual = 0,
  CodeFix = 1,
  Refactor = 2,
  Explain = 3,
  GenerateTests = 4,
  WriteDocs = 5,
};

enum class PromptDetailLevel {
  Concise = 0,
  Standard = 1,
  Detailed = 2,
};

enum PromptScenarioFlags : unsigned int {
  ScenarioExplainCode = 1 << 0,
  ScenarioFixBugs = 1 << 1,
  ScenarioRefactor = 1 << 2,
  ScenarioGenerateTests = 1 << 3,
  ScenarioWriteDocs = 1 << 4,
};

enum class TextId {
  PluginOpenPanel,
  PluginExplain,
  PluginRefactor,
  PluginComments,
  PluginFix,
  PluginSettings,
  ContextExplain,
  ContextRefactor,
  ContextComments,
  ContextFix,
  PanelTitle,
  PanelSettingsButton,
  PanelClearButton,
  PanelSendButton,
  WelcomeMessage,
  SelectTextWarning,
  SettingsTitle,
  SettingsOpenAIKeyLabel,
  SettingsGeminiKeyLabel,
  SettingsClaudeKeyLabel,
  SettingsDefaultProviderGroup,
  SettingsDefaultProviderLabel,
  SettingsLanguageLabel,
  SettingsCtrlEnter,
  SettingsApiKeysGroup,
  SettingsApiKeysOpenAIHint,
  SettingsApiKeysGeminiHint,
  SettingsApiKeysClaudeHint,
  SettingsTestConnection,
  SettingsOk,
  SettingsCancel,
};

struct ChatMessage {
  bool isUser = false;
  std::wstring author;
  std::wstring content;
  std::wstring timestamp;
};

enum class SelectionAction {
  Explain,
  ReplaceSelection,
};

struct SelectionContext {
  HWND scintilla = nullptr;
  Sci_Position start = 0;
  Sci_Position end = 0;
  int codePage = 0;
  std::wstring text;
};

struct AIAssistantConfig {
  std::wstring openAIKey;
  std::wstring geminiKey;
  std::wstring claudeKey;
  std::wstring copilotKey;
  std::wstring customPromptInstructions;
  LLMProvider defaultProvider = LLMProvider::OpenAI;
  UiLanguagePreference uiLanguagePreference = UiLanguagePreference::FollowNotepad;
  PromptResponseLanguage responseLanguage =
      PromptResponseLanguage::FollowInterface;
  PromptEncodingPreference encodingPreference =
      PromptEncodingPreference::CurrentDocument;
  PromptPreset promptPreset = PromptPreset::Manual;
  PromptDetailLevel detailLevel = PromptDetailLevel::Standard;
  unsigned int scenarioFlags = 0;
  bool outputCodeOnly = false;
  bool outputPreserveStyle = true;
  bool outputMentionRisks = false;
  bool requireCtrlEnterToSend = false;
};

HINSTANCE g_hInst = nullptr;
NppData g_nppData{};
FuncItem g_funcItems[kMenuCount]{};
HWND g_panel = nullptr;
bool g_panelRegistered = false;
bool g_panelVisible = false;
std::wstring g_moduleFileName;
std::vector<ChatMessage> g_chatHistory;
LLMProvider g_currentProvider = LLMProvider::OpenAI;
AIAssistantConfig g_config{};
std::wstring g_currentModel;
std::wstring g_lastPromptUserRequest;
HFONT g_chatFont = nullptr;
int g_fontSize = kDefaultFontSize;
CopilotTokens g_copilotTokens{};
CopilotDeviceCode g_copilotDeviceCode{};
bool g_copilotAuthInProgress = false;
DWORD g_copilotPollIntervalMs = kDefaultCopilotPollMs;
DWORD g_copilotLastPendingTick = 0;
std::wstring g_lastCopilotDebug;
std::array<WNDPROC, 2> g_originalSciWndProc{};
std::array<HWND, 2> g_scintillaWindows{};
HWND g_inputEdit = nullptr;
WNDPROC g_originalInputEditProc = nullptr;
UiLanguage g_uiLanguage = UiLanguage::English;

const wchar_t *kOpenAIKeyName = L"openai_apikey";
const wchar_t *kGeminiKeyName = L"gemini_apikey";
const wchar_t *kClaudeKeyName = L"claude_apikey";
const wchar_t *kCopilotKeyName = L"copilot_apikey";
const wchar_t *kCopilotOauthKeyName = L"copilot_oauth_token";
const wchar_t *kDefaultProviderName = L"default_provider";
const wchar_t *kUiLanguagePreferenceName = L"ui_language_preference";
const wchar_t *kResponseLanguageName = L"prompt_response_language";
const wchar_t *kEncodingPreferenceName = L"prompt_encoding_preference";
const wchar_t *kPromptPresetName = L"prompt_preset";
const wchar_t *kDetailLevelName = L"prompt_detail_level";
const wchar_t *kScenarioFlagsName = L"prompt_scenario_flags";
const wchar_t *kOutputCodeOnlyName = L"prompt_output_code_only";
const wchar_t *kOutputPreserveStyleName = L"prompt_output_preserve_style";
const wchar_t *kOutputMentionRisksName = L"prompt_output_mention_risks";
const wchar_t *kCustomPromptInstructionsName = L"prompt_custom_instructions";
const wchar_t *kRequireCtrlEnterName = L"require_ctrl_enter";

void cmdTogglePanel();
void cmdExplainSelection();
void cmdRefactorSelection();
void cmdAddComments();
void cmdFixCode();
void cmdSettings();
void commandMenuInit();
INT_PTR CALLBACK PanelDlgProc(HWND hwnd, UINT message, WPARAM wParam,
                              LPARAM lParam);
INT_PTR CALLBACK SettingsDlgProc(HWND hwnd, UINT message, WPARAM wParam,
                                 LPARAM lParam);
LRESULT CALLBACK ScintillaSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                       LPARAM lParam);
LRESULT CALLBACK InputEditSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                       LPARAM lParam);

std::wstring getProviderName(LLMProvider provider) {
  switch (provider) {
  case LLMProvider::OpenAI:
    return L"OpenAI";
  case LLMProvider::Gemini:
    return L"Gemini";
  case LLMProvider::Claude:
    return L"Claude";
  case LLMProvider::Copilot:
    return L"Copilot";
  default:
    return L"AI";
  }
}

std::wstring getCurrentNativeLangFileName() {
  if (!g_nppData._nppHandle) {
    return L"";
  }

  const LRESULT length = ::SendMessageW(g_nppData._nppHandle,
                                        NPPM_GETNATIVELANGFILENAME, 0, 0);
  if (length <= 0) {
    return L"";
  }

  std::string buffer(static_cast<size_t>(length) + 1, '\0');
  ::SendMessageW(g_nppData._nppHandle, NPPM_GETNATIVELANGFILENAME, buffer.size(),
                 reinterpret_cast<LPARAM>(buffer.data()));

  const size_t nulPos = buffer.find('\0');
  if (nulPos != std::string::npos) {
    buffer.resize(nulPos);
  }

  return std::wstring(buffer.begin(), buffer.end());
}

void refreshUiLanguage() {
  if (g_config.uiLanguagePreference == UiLanguagePreference::English) {
    g_uiLanguage = UiLanguage::English;
    return;
  }
  if (g_config.uiLanguagePreference == UiLanguagePreference::Chinese) {
    g_uiLanguage = UiLanguage::Chinese;
    return;
  }

  std::wstring langFile = getCurrentNativeLangFileName();
  std::transform(langFile.begin(), langFile.end(), langFile.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });

  if (langFile.find(L"chinese") != std::wstring::npos ||
      langFile.find(L"taiwan") != std::wstring::npos) {
    g_uiLanguage = UiLanguage::Chinese;
  } else {
    g_uiLanguage = UiLanguage::English;
  }
}

const wchar_t *tr(TextId id) {
  if (g_uiLanguage == UiLanguage::Chinese) {
    switch (id) {
    case TextId::PluginOpenPanel:
      return L"\u958B\u555F AI \u52A9\u7406";
    case TextId::PluginExplain:
      return L"\u89E3\u91CB\u9078\u53D6\u5167\u5BB9";
    case TextId::PluginRefactor:
      return L"\u91CD\u69CB\u9078\u53D6\u5167\u5BB9";
    case TextId::PluginComments:
      return L"\u70BA\u9078\u53D6\u5167\u5BB9\u52A0\u4E0A\u8A3B\u89E3";
    case TextId::PluginFix:
      return L"\u4FEE\u6B63\u9078\u53D6\u5167\u5BB9";
    case TextId::PluginSettings:
      return L"\u8A2D\u5B9A...";
    case TextId::ContextExplain:
      return L"AI\uFF1A\u89E3\u91CB\u9078\u53D6\u5167\u5BB9";
    case TextId::ContextRefactor:
      return L"AI\uFF1A\u91CD\u69CB\u9078\u53D6\u5167\u5BB9";
    case TextId::ContextComments:
      return L"AI\uFF1A\u70BA\u9078\u53D6\u5167\u5BB9\u52A0\u4E0A\u8A3B\u89E3";
    case TextId::ContextFix:
      return L"AI\uFF1A\u4FEE\u6B63\u9078\u53D6\u5167\u5BB9";
    case TextId::PanelTitle:
      return L"AI \u52A9\u7406";
    case TextId::PanelSettingsButton:
      return L"\u8A2D\u5B9A";
    case TextId::PanelClearButton:
      return L"\u6E05\u9664";
    case TextId::PanelSendButton:
      return L"\u9001\u51FA";
    case TextId::WelcomeMessage:
      return L"\u6B61\u8FCE\u4F7F\u7528 AI \u52A9\u7406\u3002\n\u53EF\u4F7F\u7528 Plugins \u9078\u55AE\u3001\u53F3\u9375\u9078\u53D6\u6587\u5B57\u6216\u76F4\u63A5\u5728\u9019\u88E1\u8F38\u5165\u3002";
    case TextId::SelectTextWarning:
      return L"\u8ACB\u5148\u5728\u7DE8\u8F2F\u5668\u4E2D\u9078\u53D6\u4E00\u6BB5\u6587\u5B57\u518D\u4F7F\u7528\u6B64\u529F\u80FD\u3002";
    case TextId::SettingsTitle:
      return L"AI \u52A9\u7406\u8A2D\u5B9A";
    case TextId::SettingsOpenAIKeyLabel:
      return L"OpenAI API Key:";
    case TextId::SettingsGeminiKeyLabel:
      return L"Gemini API Key:";
    case TextId::SettingsClaudeKeyLabel:
      return L"Claude API Key:";
    case TextId::SettingsDefaultProviderGroup:
      return L"\u9810\u8A2D\u4F9B\u61C9\u5546";
    case TextId::SettingsDefaultProviderLabel:
      return L"\u9078\u64C7\u9810\u8A2D AI \u4F9B\u61C9\u5546:";
    case TextId::SettingsLanguageLabel:
      return L"\u8A9E\u8A00\uff1A";
    case TextId::SettingsCtrlEnter:
      return L"\u5F9E AI \u9762\u677F\u9001\u51FA\u6642\u9700\u8981 Ctrl+Enter";
    case TextId::SettingsApiKeysGroup:
      return L"\u53D6\u5F97 API Key \u8CC7\u8A0A";
    case TextId::SettingsApiKeysOpenAIHint:
      return L"OpenAI: platform.openai.com - API Keys";
    case TextId::SettingsApiKeysGeminiHint:
      return L"Gemini: ai.google.dev - Get API Key";
    case TextId::SettingsApiKeysClaudeHint:
      return L"Claude: console.anthropic.com - API Keys";
    case TextId::SettingsTestConnection:
      return L"\u6E2C\u8A66\u9810\u8A2D\u9023\u7DDA";
    case TextId::SettingsOk:
      return L"\u78BA\u5B9A";
    case TextId::SettingsCancel:
      return L"\u53D6\u6D88";
    }
  }

  switch (id) {
  case TextId::PluginOpenPanel:
    return L"Open AI Assistant";
  case TextId::PluginExplain:
    return L"Explain Selection";
  case TextId::PluginRefactor:
    return L"Refactor Selection";
  case TextId::PluginComments:
    return L"Add Comments to Selection";
  case TextId::PluginFix:
    return L"Fix Selection";
  case TextId::PluginSettings:
    return L"Settings...";
  case TextId::ContextExplain:
    return L"AI: Explain Selection";
  case TextId::ContextRefactor:
    return L"AI: Refactor Selection";
  case TextId::ContextComments:
    return L"AI: Add Comments";
  case TextId::ContextFix:
    return L"AI: Fix Selection";
  case TextId::PanelTitle:
    return L"AI Assistant";
  case TextId::PanelSettingsButton:
    return L"Settings";
  case TextId::PanelClearButton:
    return L"Clear";
  case TextId::PanelSendButton:
    return L"Send";
  case TextId::WelcomeMessage:
    return L"Welcome to AI Assistant.\nUse the plugin menu, right-click selected text, or type directly here.";
  case TextId::SelectTextWarning:
    return L"Select some text in the editor before using this command.";
  case TextId::SettingsTitle:
    return L"AI Assistant Settings";
  case TextId::SettingsOpenAIKeyLabel:
    return L"OpenAI API Key:";
  case TextId::SettingsGeminiKeyLabel:
    return L"Gemini API Key:";
  case TextId::SettingsClaudeKeyLabel:
    return L"Claude API Key:";
  case TextId::SettingsDefaultProviderGroup:
    return L"Default Provider";
  case TextId::SettingsDefaultProviderLabel:
    return L"Select default AI provider:";
  case TextId::SettingsLanguageLabel:
    return L"Language:";
  case TextId::SettingsCtrlEnter:
    return L"Require Ctrl+Enter to send from AI panel";
  case TextId::SettingsApiKeysGroup:
    return L"How to get API Keys";
  case TextId::SettingsApiKeysOpenAIHint:
    return L"OpenAI: platform.openai.com - API Keys";
  case TextId::SettingsApiKeysGeminiHint:
    return L"Gemini: ai.google.dev - Get API Key";
  case TextId::SettingsApiKeysClaudeHint:
    return L"Claude: console.anthropic.com - API Keys";
  case TextId::SettingsTestConnection:
    return L"Test Default Connection";
  case TextId::SettingsOk:
    return L"OK";
  case TextId::SettingsCancel:
    return L"Cancel";
  }

  return L"";
}

void setCommand(size_t index, const wchar_t *name, PFUNCPLUGINCMD fn) {
  wcscpy_s(g_funcItems[index]._itemName, name);
  g_funcItems[index]._pFunc = fn;
  g_funcItems[index]._init2Check = false;
  g_funcItems[index]._pShKey = nullptr;
}

bool isProviderEnabled(LLMProvider provider) {
  return std::find(kEnabledProviders.begin(), kEnabledProviders.end(), provider) !=
         kEnabledProviders.end();
}

LLMProvider sanitizeProvider(LLMProvider provider) {
  return isProviderEnabled(provider) ? provider : LLMProvider::OpenAI;
}

UiLanguagePreference sanitizeUiLanguagePreference(int rawValue) {
  switch (rawValue) {
  case static_cast<int>(UiLanguagePreference::FollowNotepad):
    return UiLanguagePreference::FollowNotepad;
  case static_cast<int>(UiLanguagePreference::English):
    return UiLanguagePreference::English;
  case static_cast<int>(UiLanguagePreference::Chinese):
    return UiLanguagePreference::Chinese;
  default:
    return UiLanguagePreference::FollowNotepad;
  }
}

PromptResponseLanguage sanitizePromptResponseLanguage(int rawValue) {
  switch (rawValue) {
  case static_cast<int>(PromptResponseLanguage::FollowInterface):
    return PromptResponseLanguage::FollowInterface;
  case static_cast<int>(PromptResponseLanguage::TraditionalChinese):
    return PromptResponseLanguage::TraditionalChinese;
  case static_cast<int>(PromptResponseLanguage::English):
    return PromptResponseLanguage::English;
  default:
    return PromptResponseLanguage::FollowInterface;
  }
}

PromptEncodingPreference sanitizePromptEncodingPreference(int rawValue) {
  switch (rawValue) {
  case static_cast<int>(PromptEncodingPreference::CurrentDocument):
    return PromptEncodingPreference::CurrentDocument;
  case static_cast<int>(PromptEncodingPreference::UTF8):
    return PromptEncodingPreference::UTF8;
  case static_cast<int>(PromptEncodingPreference::UTF8Bom):
    return PromptEncodingPreference::UTF8Bom;
  case static_cast<int>(PromptEncodingPreference::Big5):
    return PromptEncodingPreference::Big5;
  case static_cast<int>(PromptEncodingPreference::ANSI):
    return PromptEncodingPreference::ANSI;
  default:
    return PromptEncodingPreference::CurrentDocument;
  }
}

PromptPreset sanitizePromptPreset(int rawValue) {
  switch (rawValue) {
  case static_cast<int>(PromptPreset::Manual):
    return PromptPreset::Manual;
  case static_cast<int>(PromptPreset::CodeFix):
    return PromptPreset::CodeFix;
  case static_cast<int>(PromptPreset::Refactor):
    return PromptPreset::Refactor;
  case static_cast<int>(PromptPreset::Explain):
    return PromptPreset::Explain;
  case static_cast<int>(PromptPreset::GenerateTests):
    return PromptPreset::GenerateTests;
  case static_cast<int>(PromptPreset::WriteDocs):
    return PromptPreset::WriteDocs;
  default:
    return PromptPreset::Manual;
  }
}
PromptDetailLevel sanitizePromptDetailLevel(int rawValue) {
  switch (rawValue) {
  case static_cast<int>(PromptDetailLevel::Concise):
    return PromptDetailLevel::Concise;
  case static_cast<int>(PromptDetailLevel::Standard):
    return PromptDetailLevel::Standard;
  case static_cast<int>(PromptDetailLevel::Detailed):
    return PromptDetailLevel::Detailed;
  default:
    return PromptDetailLevel::Standard;
  }
}

int providerToComboIndex(LLMProvider provider) {
  auto it = std::find(kEnabledProviders.begin(), kEnabledProviders.end(), provider);
  if (it == kEnabledProviders.end()) {
    return 0;
  }

  return static_cast<int>(std::distance(kEnabledProviders.begin(), it));
}

LLMProvider comboIndexToProvider(int index) {
  if (index < 0 || index >= static_cast<int>(kEnabledProviders.size())) {
    return LLMProvider::OpenAI;
  }

  return kEnabledProviders[static_cast<size_t>(index)];
}

int uiLanguagePreferenceToComboIndex(UiLanguagePreference preference) {
  switch (preference) {
  case UiLanguagePreference::FollowNotepad:
    return 0;
  case UiLanguagePreference::English:
    return 1;
  case UiLanguagePreference::Chinese:
    return 2;
  default:
    return 0;
  }
}

UiLanguagePreference comboIndexToUiLanguagePreference(int index) {
  switch (index) {
  case 1:
    return UiLanguagePreference::English;
  case 2:
    return UiLanguagePreference::Chinese;
  default:
    return UiLanguagePreference::FollowNotepad;
  }
}

std::wstring getUiLanguageOptionText(UiLanguagePreference preference) {
  switch (preference) {
  case UiLanguagePreference::FollowNotepad:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u8DDF\u96A8 Notepad++"
                                               : L"Follow Notepad++";
  case UiLanguagePreference::English:
    return L"English";
  case UiLanguagePreference::Chinese:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u4E2D\u6587" : L"Chinese";
  default:
    return L"";
  }
}

int promptPresetToComboIndex(PromptPreset preset) {
  return static_cast<int>(preset);
}

PromptPreset comboIndexToPromptPreset(int index) {
  return sanitizePromptPreset(index);
}

std::wstring getPromptPresetOptionText(PromptPreset preset) {
  switch (preset) {
  case PromptPreset::CodeFix:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u7A0B\u5F0F\u4FEE\u6B63" : L"Code Fix";
  case PromptPreset::Refactor:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u91CD\u69CB\u512A\u5316" : L"Refactor";
  case PromptPreset::Explain:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u7A0B\u5F0F\u89E3\u8AAA" : L"Explain Code";
  case PromptPreset::GenerateTests:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u6E2C\u8A66\u751F\u6210" : L"Generate Tests";
  case PromptPreset::WriteDocs:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u6587\u4EF6\u64B0\u5BEB" : L"Write Docs";
  case PromptPreset::Manual:
  default:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u81EA\u8A02" : L"Custom";
  }
}

void populatePromptPresetCombo(HWND combo, PromptPreset selected) {
  if (!combo) {
    return;
  }

  ::SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  std::array<PromptPreset, 6> options = {PromptPreset::Manual,
                                         PromptPreset::CodeFix,
                                         PromptPreset::Refactor,
                                         PromptPreset::Explain,
                                         PromptPreset::GenerateTests,
                                         PromptPreset::WriteDocs};
  for (PromptPreset option : options) {
    std::wstring text = getPromptPresetOptionText(option);
    ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
  }
  ::SendMessageW(combo, CB_SETCURSEL,
                 static_cast<WPARAM>(promptPresetToComboIndex(selected)), 0);
}
int promptResponseLanguageToComboIndex(PromptResponseLanguage language) {
  return static_cast<int>(language);
}

PromptResponseLanguage comboIndexToPromptResponseLanguage(int index) {
  return sanitizePromptResponseLanguage(index);
}

std::wstring getPromptResponseLanguageOptionText(PromptResponseLanguage language) {
  switch (language) {
  case PromptResponseLanguage::FollowInterface:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u8DDF\u96A8\u4ECB\u9762\u8A9E\u8A00"
                                               : L"Follow interface language";
  case PromptResponseLanguage::TraditionalChinese:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u7E41\u9AD4\u4E2D\u6587"
                                               : L"Traditional Chinese";
  case PromptResponseLanguage::English:
    return L"English";
  default:
    return L"";
  }
}

int promptEncodingPreferenceToComboIndex(PromptEncodingPreference preference) {
  return static_cast<int>(preference);
}

PromptEncodingPreference comboIndexToPromptEncodingPreference(int index) {
  return sanitizePromptEncodingPreference(index);
}

std::wstring getPromptEncodingOptionText(PromptEncodingPreference preference) {
  switch (preference) {
  case PromptEncodingPreference::CurrentDocument:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u8DDF\u96A8\u76EE\u524D\u6587\u4EF6"
                                               : L"Follow current document";
  case PromptEncodingPreference::UTF8:
    return L"UTF-8";
  case PromptEncodingPreference::UTF8Bom:
    return L"UTF-8 BOM";
  case PromptEncodingPreference::Big5:
    return L"Big5";
  case PromptEncodingPreference::ANSI:
    return L"ANSI";
  default:
    return L"";
  }
}

int promptDetailLevelToComboIndex(PromptDetailLevel level) {
  return static_cast<int>(level);
}

PromptDetailLevel comboIndexToPromptDetailLevel(int index) {
  return sanitizePromptDetailLevel(index);
}

std::wstring getPromptDetailLevelOptionText(PromptDetailLevel level) {
  switch (level) {
  case PromptDetailLevel::Concise:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u7C21\u6F54" : L"Concise";
  case PromptDetailLevel::Standard:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u6A19\u6E96" : L"Standard";
  case PromptDetailLevel::Detailed:
    return g_uiLanguage == UiLanguage::Chinese ? L"\u8A73\u7D30" : L"Detailed";
  default:
    return L"";
  }
}

void populatePromptResponseLanguageCombo(HWND combo,
                                         PromptResponseLanguage selected) {
  if (!combo) {
    return;
  }

  ::SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  std::array<PromptResponseLanguage, 3> options = {
      PromptResponseLanguage::FollowInterface,
      PromptResponseLanguage::TraditionalChinese,
      PromptResponseLanguage::English};
  for (PromptResponseLanguage option : options) {
    std::wstring text = getPromptResponseLanguageOptionText(option);
    ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
  }
  ::SendMessageW(combo, CB_SETCURSEL,
                 static_cast<WPARAM>(promptResponseLanguageToComboIndex(selected)),
                 0);
}

void populatePromptEncodingCombo(HWND combo, PromptEncodingPreference selected) {
  if (!combo) {
    return;
  }

  ::SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  std::array<PromptEncodingPreference, 5> options = {
      PromptEncodingPreference::CurrentDocument, PromptEncodingPreference::UTF8,
      PromptEncodingPreference::UTF8Bom, PromptEncodingPreference::Big5,
      PromptEncodingPreference::ANSI};
  for (PromptEncodingPreference option : options) {
    std::wstring text = getPromptEncodingOptionText(option);
    ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
  }
  ::SendMessageW(combo, CB_SETCURSEL,
                 static_cast<WPARAM>(promptEncodingPreferenceToComboIndex(selected)),
                 0);
}

void populatePromptDetailCombo(HWND combo, PromptDetailLevel selected) {
  if (!combo) {
    return;
  }

  ::SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  std::array<PromptDetailLevel, 3> options = {PromptDetailLevel::Concise,
                                              PromptDetailLevel::Standard,
                                              PromptDetailLevel::Detailed};
  for (PromptDetailLevel option : options) {
    std::wstring text = getPromptDetailLevelOptionText(option);
    ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
  }
  ::SendMessageW(combo, CB_SETCURSEL,
                 static_cast<WPARAM>(promptDetailLevelToComboIndex(selected)), 0);
}

void applyPromptPresetToConfig(AIAssistantConfig &config, PromptPreset preset) {
  config.promptPreset = preset;
  switch (preset) {
  case PromptPreset::CodeFix:
    config.responseLanguage = PromptResponseLanguage::FollowInterface;
    config.encodingPreference = PromptEncodingPreference::CurrentDocument;
    config.detailLevel = PromptDetailLevel::Standard;
    config.scenarioFlags = ScenarioFixBugs;
    config.outputCodeOnly = true;
    config.outputPreserveStyle = true;
    config.outputMentionRisks = true;
    break;
  case PromptPreset::Refactor:
    config.responseLanguage = PromptResponseLanguage::FollowInterface;
    config.encodingPreference = PromptEncodingPreference::CurrentDocument;
    config.detailLevel = PromptDetailLevel::Standard;
    config.scenarioFlags = ScenarioRefactor;
    config.outputCodeOnly = true;
    config.outputPreserveStyle = true;
    config.outputMentionRisks = true;
    break;
  case PromptPreset::Explain:
    config.responseLanguage = PromptResponseLanguage::FollowInterface;
    config.encodingPreference = PromptEncodingPreference::CurrentDocument;
    config.detailLevel = PromptDetailLevel::Detailed;
    config.scenarioFlags = ScenarioExplainCode;
    config.outputCodeOnly = false;
    config.outputPreserveStyle = true;
    config.outputMentionRisks = false;
    break;
  case PromptPreset::GenerateTests:
    config.responseLanguage = PromptResponseLanguage::FollowInterface;
    config.encodingPreference = PromptEncodingPreference::CurrentDocument;
    config.detailLevel = PromptDetailLevel::Standard;
    config.scenarioFlags = ScenarioGenerateTests;
    config.outputCodeOnly = true;
    config.outputPreserveStyle = true;
    config.outputMentionRisks = true;
    break;
  case PromptPreset::WriteDocs:
    config.responseLanguage = PromptResponseLanguage::FollowInterface;
    config.encodingPreference = PromptEncodingPreference::CurrentDocument;
    config.detailLevel = PromptDetailLevel::Standard;
    config.scenarioFlags = ScenarioWriteDocs;
    config.outputCodeOnly = false;
    config.outputPreserveStyle = true;
    config.outputMentionRisks = false;
    break;
  case PromptPreset::Manual:
  default:
    break;
  }
}

void syncPromptControlsFromConfig(HWND hwnd, const AIAssistantConfig &config) {
  populatePromptPresetCombo(::GetDlgItem(hwnd, IDC_PROMPT_PRESET_COMBO),
                            config.promptPreset);
  populatePromptResponseLanguageCombo(
      ::GetDlgItem(hwnd, IDC_RESPONSE_LANGUAGE_COMBO), config.responseLanguage);
  populatePromptEncodingCombo(::GetDlgItem(hwnd, IDC_ENCODING_COMBO),
                              config.encodingPreference);
  populatePromptDetailCombo(::GetDlgItem(hwnd, IDC_DETAIL_LEVEL_COMBO),
                            config.detailLevel);
  ::SendMessageW(::GetDlgItem(hwnd, IDC_SCENARIO_EXPLAIN_CHECK), BM_SETCHECK,
                 (config.scenarioFlags & ScenarioExplainCode) ? BST_CHECKED
                                                              : BST_UNCHECKED,
                 0);
  ::SendMessageW(::GetDlgItem(hwnd, IDC_SCENARIO_FIX_CHECK), BM_SETCHECK,
                 (config.scenarioFlags & ScenarioFixBugs) ? BST_CHECKED
                                                          : BST_UNCHECKED,
                 0);
  ::SendMessageW(::GetDlgItem(hwnd, IDC_SCENARIO_REFACTOR_CHECK), BM_SETCHECK,
                 (config.scenarioFlags & ScenarioRefactor) ? BST_CHECKED
                                                           : BST_UNCHECKED,
                 0);
  ::SendMessageW(::GetDlgItem(hwnd, IDC_SCENARIO_TEST_CHECK), BM_SETCHECK,
                 (config.scenarioFlags & ScenarioGenerateTests) ? BST_CHECKED
                                                                : BST_UNCHECKED,
                 0);
  ::SendMessageW(::GetDlgItem(hwnd, IDC_SCENARIO_DOC_CHECK), BM_SETCHECK,
                 (config.scenarioFlags & ScenarioWriteDocs) ? BST_CHECKED
                                                            : BST_UNCHECKED,
                 0);
  ::SendMessageW(::GetDlgItem(hwnd, IDC_OUTPUT_CODE_ONLY_CHECK), BM_SETCHECK,
                 config.outputCodeOnly ? BST_CHECKED : BST_UNCHECKED, 0);
  ::SendMessageW(::GetDlgItem(hwnd, IDC_OUTPUT_PRESERVE_STYLE_CHECK), BM_SETCHECK,
                 config.outputPreserveStyle ? BST_CHECKED : BST_UNCHECKED, 0);
  ::SendMessageW(::GetDlgItem(hwnd, IDC_OUTPUT_RISKS_CHECK), BM_SETCHECK,
                 config.outputMentionRisks ? BST_CHECKED : BST_UNCHECKED, 0);
}

void capturePromptSettingsFromDialog(HWND hwnd, AIAssistantConfig &config) {
  config.requireCtrlEnterToSend =
      ::SendMessageW(::GetDlgItem(hwnd, IDC_SEND_SHORTCUT_CHECK), BM_GETCHECK, 0,
                     0) == BST_CHECKED;
  int languageSelection = static_cast<int>(::SendMessageW(
      ::GetDlgItem(hwnd, IDC_UI_LANGUAGE_COMBO), CB_GETCURSEL, 0, 0));
  config.uiLanguagePreference =
      comboIndexToUiLanguagePreference(languageSelection);
  int providerSelection = static_cast<int>(::SendMessageW(
      ::GetDlgItem(hwnd, IDC_DEFAULT_PROVIDER_COMBO), CB_GETCURSEL, 0, 0));
  if (providerSelection >= 0 &&
      providerSelection < static_cast<int>(kEnabledProviders.size())) {
    config.defaultProvider = comboIndexToProvider(providerSelection);
  }
  int presetSelection = static_cast<int>(::SendMessageW(
      ::GetDlgItem(hwnd, IDC_PROMPT_PRESET_COMBO), CB_GETCURSEL, 0, 0));
  config.promptPreset = comboIndexToPromptPreset(presetSelection);
  int responseLanguageSelection = static_cast<int>(::SendMessageW(
      ::GetDlgItem(hwnd, IDC_RESPONSE_LANGUAGE_COMBO), CB_GETCURSEL, 0, 0));
  config.responseLanguage =
      comboIndexToPromptResponseLanguage(responseLanguageSelection);
  int encodingSelection = static_cast<int>(::SendMessageW(
      ::GetDlgItem(hwnd, IDC_ENCODING_COMBO), CB_GETCURSEL, 0, 0));
  config.encodingPreference =
      comboIndexToPromptEncodingPreference(encodingSelection);
  int detailSelection = static_cast<int>(::SendMessageW(
      ::GetDlgItem(hwnd, IDC_DETAIL_LEVEL_COMBO), CB_GETCURSEL, 0, 0));
  config.detailLevel = comboIndexToPromptDetailLevel(detailSelection);
  unsigned int scenarioFlags = 0;
  if (::SendMessageW(::GetDlgItem(hwnd, IDC_SCENARIO_EXPLAIN_CHECK),
                     BM_GETCHECK, 0, 0) == BST_CHECKED) {
    scenarioFlags |= ScenarioExplainCode;
  }
  if (::SendMessageW(::GetDlgItem(hwnd, IDC_SCENARIO_FIX_CHECK),
                     BM_GETCHECK, 0, 0) == BST_CHECKED) {
    scenarioFlags |= ScenarioFixBugs;
  }
  if (::SendMessageW(::GetDlgItem(hwnd, IDC_SCENARIO_REFACTOR_CHECK),
                     BM_GETCHECK, 0, 0) == BST_CHECKED) {
    scenarioFlags |= ScenarioRefactor;
  }
  if (::SendMessageW(::GetDlgItem(hwnd, IDC_SCENARIO_TEST_CHECK),
                     BM_GETCHECK, 0, 0) == BST_CHECKED) {
    scenarioFlags |= ScenarioGenerateTests;
  }
  if (::SendMessageW(::GetDlgItem(hwnd, IDC_SCENARIO_DOC_CHECK),
                     BM_GETCHECK, 0, 0) == BST_CHECKED) {
    scenarioFlags |= ScenarioWriteDocs;
  }
  config.scenarioFlags = scenarioFlags;
  config.outputCodeOnly =
      ::SendMessageW(::GetDlgItem(hwnd, IDC_OUTPUT_CODE_ONLY_CHECK), BM_GETCHECK,
                     0, 0) == BST_CHECKED;
  config.outputPreserveStyle =
      ::SendMessageW(::GetDlgItem(hwnd, IDC_OUTPUT_PRESERVE_STYLE_CHECK),
                     BM_GETCHECK, 0, 0) == BST_CHECKED;
  config.outputMentionRisks =
      ::SendMessageW(::GetDlgItem(hwnd, IDC_OUTPUT_RISKS_CHECK), BM_GETCHECK, 0,
                     0) == BST_CHECKED;
  config.customPromptInstructions.clear();
}

std::wstring trimWhitespace(const std::wstring &value) {
  size_t start = 0;
  while (start < value.size() && iswspace(value[start])) {
    ++start;
  }

  size_t end = value.size();
  while (end > start && iswspace(value[end - 1])) {
    --end;
  }

  return value.substr(start, end - start);
}

std::wstring toLowerAscii(const std::wstring &value) {
  std::wstring lower = value;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
  return lower;
}

bool containsCaseInsensitive(const std::wstring &text,
                             const std::wstring &needle) {
  if (needle.empty()) {
    return false;
  }

  const std::wstring lowerText = toLowerAscii(text);
  const std::wstring lowerNeedle = toLowerAscii(needle);
  return lowerText.find(lowerNeedle) != std::wstring::npos;
}

bool isInterruptedConversationMessage(const std::wstring &message) {
  return containsCaseInsensitive(message, L"conversation interrupted") ||
         containsCaseInsensitive(
             message, L"tell the model what to do differently") ||
         containsCaseInsensitive(message, L"something went wrong") ||
         containsCaseInsensitive(message, L"/feedback");
}

std::wstring buildInterruptedConversationNotice() {
  if (g_uiLanguage == UiLanguage::Chinese) {
    return L"[\u932F\u8AA4] AI \u8ACB\u6C42\u88AB\u4E0A\u6E38\u670D\u52D9\u4E2D\u65B7\uff0c\u5DF2\u81EA\u52D5\u91CD\u8A66\uff0c\u8ACB\u518D\u8A66\u4E00\u6B21\u3002";
  }
  return L"[Error] AI request was interrupted by the upstream service. It was retried automatically. Please try again.";
}

std::wstring normalizeLineEndings(const std::wstring &value) {
  std::wstring normalized;
  normalized.reserve(value.size());

  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == L'\r') {
      if (i + 1 < value.size() && value[i + 1] == L'\n') {
        ++i;
      }
      normalized += L'\n';
    } else {
      normalized += value[i];
    }
  }

  return normalized;
}

bool isOrderedListLine(const std::wstring &trimmed) {
  size_t pos = 0;
  while (pos < trimmed.size() && iswdigit(trimmed[pos])) {
    ++pos;
  }

  return pos > 0 && pos + 1 < trimmed.size() && trimmed[pos] == L'.' &&
         trimmed[pos + 1] == L' ';
}

bool isBulletLine(const std::wstring &trimmed) {
  if (trimmed.size() >= 2 &&
      ((trimmed[0] == L'-' || trimmed[0] == L'*' || trimmed[0] == L'+') &&
       trimmed[1] == L' ')) {
    return true;
  }

  if (trimmed.size() >= 2 && trimmed[0] == 0x2022 && trimmed[1] == L' ') {
    return true;
  }

  return isOrderedListLine(trimmed);
}

std::wstring formatMessageContent(const std::wstring &content, bool isUser) {
  std::wstring normalized = normalizeLineEndings(content);
  std::wstringstream input(normalized);
  std::wstring line;
  std::wstring formatted;
  bool previousBlank = true;
  bool inCodeBlock = false;

  auto appendLine = [&](const std::wstring &text) {
    formatted += text;
    formatted += L"\r\n";
  };

  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == L'\r') {
      line.pop_back();
    }

    std::wstring trimmed = isUser ? line : trimWhitespace(line);

    if (!isUser && trimmed.rfind(L"```", 0) == 0) {
      if (!formatted.empty() && !previousBlank) {
        appendLine(L"");
      }
      appendLine(trimmed);
      inCodeBlock = !inCodeBlock;
      previousBlank = false;
      continue;
    }

    if (inCodeBlock) {
      appendLine(line);
      previousBlank = line.empty();
      continue;
    }

    if (trimmed.empty()) {
      if (!previousBlank) {
        appendLine(L"");
      }
      previousBlank = true;
      continue;
    }

    bool isHeading = !isUser && trimmed[0] == L'#';
    bool isList = !isUser && isBulletLine(trimmed);
    if (!formatted.empty() && !previousBlank && (isHeading || isList)) {
      appendLine(L"");
    }

    appendLine(trimmed);
    if (isHeading) {
      appendLine(L"");
    }
    previousBlank = false;
  }

  while (formatted.size() >= 2 &&
         formatted.compare(formatted.size() - 2, 2, L"\r\n") == 0) {
    formatted.erase(formatted.size() - 2);
  }

  return formatted;
}

std::wstring getProviderApiKey(LLMProvider provider) {
  switch (provider) {
  case LLMProvider::OpenAI:
    return trimWhitespace(g_config.openAIKey);
  case LLMProvider::Gemini:
    return trimWhitespace(g_config.geminiKey);
  case LLMProvider::Claude:
    return trimWhitespace(g_config.claudeKey);
  default:
    return L"";
  }
}

void addMessage(bool isUser, const std::wstring &content) {
  ChatMessage msg;
  msg.isUser = isUser;
  msg.author = isUser ? L"You" : getProviderName(g_currentProvider);
  msg.content = content;

  time_t now = time(nullptr);
  tm localTime{};
  wchar_t timeText[64] = L"--:--:--";
  if (localtime_s(&localTime, &now) == 0) {
    wcsftime(timeText, 64, L"%H:%M:%S", &localTime);
  }
  msg.timestamp = timeText;
  g_chatHistory.push_back(msg);
}

void updateChatDisplay() {
  if (!g_panel) {
    return;
  }

  HWND chat = ::GetDlgItem(g_panel, IDC_AI_CHAT_HISTORY);
  if (!chat) {
    return;
  }

  std::wstringstream stream;
  for (const auto &msg : g_chatHistory) {
    stream << L"[" << msg.timestamp << L"] " << msg.author << L":\r\n"
           << formatMessageContent(msg.content, msg.isUser) << L"\r\n\r\n";
  }

  std::wstring text = stream.str();
  ::SetWindowTextW(chat, text.c_str());

  int end = static_cast<int>(::SendMessageW(chat, WM_GETTEXTLENGTH, 0, 0));
  ::SendMessageW(chat, EM_SETSEL, end, end);
  ::SendMessageW(chat, EM_SCROLLCARET, 0, 0);
}

void loadConfig() {
  g_config.openAIKey = SecureStorage::loadApiKey(kOpenAIKeyName);
  g_config.geminiKey = SecureStorage::loadApiKey(kGeminiKeyName);
  g_config.claudeKey = SecureStorage::loadApiKey(kClaudeKeyName);
  g_config.copilotKey = SecureStorage::loadApiKey(kCopilotKeyName);
  g_config.requireCtrlEnterToSend =
      SecureStorage::loadApiKey(kRequireCtrlEnterName) == L"1";
  g_config.outputCodeOnly =
      SecureStorage::loadApiKey(kOutputCodeOnlyName) == L"1";
  g_config.outputPreserveStyle =
      SecureStorage::loadApiKey(kOutputPreserveStyleName) != L"0";
  g_config.outputMentionRisks =
      SecureStorage::loadApiKey(kOutputMentionRisksName) == L"1";
  g_config.customPromptInstructions =
      SecureStorage::loadApiKey(kCustomPromptInstructionsName);

  std::wstring savedResponseLanguage =
      SecureStorage::loadApiKey(kResponseLanguageName);
  if (!savedResponseLanguage.empty()) {
    try {
      g_config.responseLanguage =
          sanitizePromptResponseLanguage(std::stoi(savedResponseLanguage));
    } catch (...) {
      g_config.responseLanguage = PromptResponseLanguage::FollowInterface;
    }
  }

  std::wstring savedEncodingPreference =
      SecureStorage::loadApiKey(kEncodingPreferenceName);
  if (!savedEncodingPreference.empty()) {
    try {
      g_config.encodingPreference =
          sanitizePromptEncodingPreference(std::stoi(savedEncodingPreference));
    } catch (...) {
      g_config.encodingPreference = PromptEncodingPreference::CurrentDocument;
    }
  }

  std::wstring savedPromptPreset =
      SecureStorage::loadApiKey(kPromptPresetName);
  if (!savedPromptPreset.empty()) {
    try {
      g_config.promptPreset = sanitizePromptPreset(std::stoi(savedPromptPreset));
    } catch (...) {
      g_config.promptPreset = PromptPreset::Manual;
    }
  }

  std::wstring savedDetailLevel = SecureStorage::loadApiKey(kDetailLevelName);
  if (!savedDetailLevel.empty()) {
    try {
      g_config.detailLevel = sanitizePromptDetailLevel(std::stoi(savedDetailLevel));
    } catch (...) {
      g_config.detailLevel = PromptDetailLevel::Standard;
    }
  }

  std::wstring savedScenarioFlags = SecureStorage::loadApiKey(kScenarioFlagsName);
  if (!savedScenarioFlags.empty()) {
    try {
      g_config.scenarioFlags = static_cast<unsigned int>(std::stoul(savedScenarioFlags));
    } catch (...) {
      g_config.scenarioFlags = 0;
    }
  }

  std::wstring savedLanguagePreference =
      SecureStorage::loadApiKey(kUiLanguagePreferenceName);
  if (!savedLanguagePreference.empty()) {
    try {
      g_config.uiLanguagePreference =
          sanitizeUiLanguagePreference(std::stoi(savedLanguagePreference));
    } catch (...) {
      g_config.uiLanguagePreference = UiLanguagePreference::FollowNotepad;
    }
  } else {
    g_config.uiLanguagePreference = UiLanguagePreference::FollowNotepad;
  }

  std::wstring savedProvider = SecureStorage::loadApiKey(kDefaultProviderName);
  if (!savedProvider.empty()) {
    try {
      int providerValue = std::stoi(savedProvider);
      if (providerValue >= 0 &&
          providerValue < static_cast<int>(LLMProvider::ProviderCount)) {
        g_config.defaultProvider = sanitizeProvider(
            static_cast<LLMProvider>(providerValue));
      }
    } catch (...) {
      g_config.defaultProvider = LLMProvider::OpenAI;
    }
  } else {
    g_config.defaultProvider = sanitizeProvider(g_config.defaultProvider);
  }
}

void saveConfig(const AIAssistantConfig &config) {
  SecureStorage::saveApiKey(kOpenAIKeyName, config.openAIKey);
  SecureStorage::saveApiKey(kGeminiKeyName, config.geminiKey);
  SecureStorage::saveApiKey(kClaudeKeyName, config.claudeKey);
  SecureStorage::saveApiKey(kCopilotKeyName, config.copilotKey);
  SecureStorage::saveApiKey(
      kDefaultProviderName,
      std::to_wstring(static_cast<int>(sanitizeProvider(config.defaultProvider))));
  SecureStorage::saveApiKey(
      kUiLanguagePreferenceName,
      std::to_wstring(static_cast<int>(config.uiLanguagePreference)));
  SecureStorage::saveApiKey(
      kResponseLanguageName,
      std::to_wstring(static_cast<int>(config.responseLanguage)));
  SecureStorage::saveApiKey(
      kEncodingPreferenceName,
      std::to_wstring(static_cast<int>(config.encodingPreference)));
  SecureStorage::saveApiKey(
      kPromptPresetName,
      std::to_wstring(static_cast<int>(sanitizePromptPreset(
          static_cast<int>(config.promptPreset)))));
  SecureStorage::saveApiKey(
      kDetailLevelName,
      std::to_wstring(static_cast<int>(config.detailLevel)));
  SecureStorage::saveApiKey(kScenarioFlagsName,
                            std::to_wstring(config.scenarioFlags));
  SecureStorage::saveApiKey(kOutputCodeOnlyName,
                            config.outputCodeOnly ? L"1" : L"0");
  SecureStorage::saveApiKey(kOutputPreserveStyleName,
                            config.outputPreserveStyle ? L"1" : L"0");
  SecureStorage::saveApiKey(kOutputMentionRisksName,
                            config.outputMentionRisks ? L"1" : L"0");
  SecureStorage::saveApiKey(kCustomPromptInstructionsName,
                            config.customPromptInstructions);
  SecureStorage::saveApiKey(kRequireCtrlEnterName,
                            config.requireCtrlEnterToSend ? L"1" : L"0");
  g_config = config;
  g_config.defaultProvider = sanitizeProvider(g_config.defaultProvider);
  g_config.uiLanguagePreference =
      sanitizeUiLanguagePreference(static_cast<int>(g_config.uiLanguagePreference));
  g_config.responseLanguage =
      sanitizePromptResponseLanguage(static_cast<int>(g_config.responseLanguage));
  g_config.encodingPreference = sanitizePromptEncodingPreference(
      static_cast<int>(g_config.encodingPreference));
  g_config.promptPreset =
      sanitizePromptPreset(static_cast<int>(g_config.promptPreset));
  g_config.detailLevel =
      sanitizePromptDetailLevel(static_cast<int>(g_config.detailLevel));
}

void loadCopilotToken() {
  std::wstring oauthToken = SecureStorage::loadApiKey(kCopilotOauthKeyName);
  if (oauthToken.empty()) {
    return;
  }

  g_copilotTokens.oauthToken = oauthToken;
  if (LLMApiClient::refreshCopilotToken(g_copilotTokens)) {
    g_copilotTokens.isAuthenticated = true;
  }
}

void saveCopilotToken() {
  if (!g_copilotTokens.oauthToken.empty()) {
    SecureStorage::saveApiKey(kCopilotOauthKeyName, g_copilotTokens.oauthToken);
  }
}

HWND getCurrentScintilla() {
  int which = 0;
  ::SendMessageW(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0,
                 reinterpret_cast<LPARAM>(&which));
  return which == 0 ? g_nppData._scintillaMainHandle
                    : g_nppData._scintillaSecondHandle;
}

std::wstring getSelectionText(HWND scintilla) {
  if (!scintilla) {
    return L"";
  }

  const auto selStart = static_cast<Sci_Position>(
      ::SendMessage(scintilla, SCI_GETSELECTIONSTART, 0, 0));
  const auto selEnd = static_cast<Sci_Position>(
      ::SendMessage(scintilla, SCI_GETSELECTIONEND, 0, 0));

  if (selStart == selEnd) {
    return L"";
  }

  const size_t length = static_cast<size_t>(selEnd - selStart);
  std::vector<char> buffer(length + 1, '\0');
  ::SendMessage(scintilla, SCI_GETSELTEXT, 0,
                reinterpret_cast<LPARAM>(buffer.data()));

  int codePage = static_cast<int>(::SendMessage(scintilla, SCI_GETCODEPAGE, 0, 0));
  UINT windowsCodePage = codePage > 0 ? static_cast<UINT>(codePage) : CP_ACP;

  int wideLen =
      MultiByteToWideChar(windowsCodePage, 0, buffer.data(), -1, nullptr, 0);
  if (wideLen <= 1) {
    return L"";
  }

  std::wstring result(static_cast<size_t>(wideLen - 1), L'\0');
  MultiByteToWideChar(windowsCodePage, 0, buffer.data(), -1, result.data(),
                      wideLen);
  return result;
}

std::wstring getSelectionText() { return getSelectionText(getCurrentScintilla()); }

SelectionContext getCurrentSelectionContext() {
  SelectionContext context;
  context.scintilla = getCurrentScintilla();
  if (!context.scintilla) {
    return context;
  }

  context.start = static_cast<Sci_Position>(
      ::SendMessage(context.scintilla, SCI_GETSELECTIONSTART, 0, 0));
  context.end = static_cast<Sci_Position>(
      ::SendMessage(context.scintilla, SCI_GETSELECTIONEND, 0, 0));
  if (context.start == context.end) {
    context.scintilla = nullptr;
    return context;
  }

  context.codePage =
      static_cast<int>(::SendMessage(context.scintilla, SCI_GETCODEPAGE, 0, 0));
  context.text = getSelectionText(context.scintilla);
  if (context.text.empty()) {
    context.scintilla = nullptr;
  }

  return context;
}

std::vector<char> wideToEditorText(const std::wstring &text, int codePage) {
  const UINT windowsCodePage = codePage > 0 ? static_cast<UINT>(codePage) : CP_ACP;
  int byteCount =
      ::WideCharToMultiByte(windowsCodePage, 0, text.c_str(), -1, nullptr, 0,
                            nullptr, nullptr);
  if (byteCount <= 1) {
    return {};
  }

  std::vector<char> buffer(static_cast<size_t>(byteCount), '\0');
  ::WideCharToMultiByte(windowsCodePage, 0, text.c_str(), -1, buffer.data(),
                        byteCount, nullptr, nullptr);
  return buffer;
}

bool replaceSelectionText(const SelectionContext &context,
                          const std::wstring &replacement) {
  if (!context.scintilla || replacement.empty()) {
    return false;
  }

  std::vector<char> encoded = wideToEditorText(replacement, context.codePage);
  if (encoded.empty()) {
    return false;
  }

  ::SendMessage(context.scintilla, SCI_BEGINUNDOACTION, 0, 0);
  ::SendMessage(context.scintilla, SCI_SETTARGETSTART, context.start, 0);
  ::SendMessage(context.scintilla, SCI_SETTARGETEND, context.end, 0);
  ::SendMessage(context.scintilla, SCI_REPLACETARGET, -1,
                reinterpret_cast<LPARAM>(encoded.data()));
  ::SendMessage(context.scintilla, SCI_ENDUNDOACTION, 0, 0);
  return true;
}

void clearInput() {
  if (g_panel) {
    ::SetWindowTextW(::GetDlgItem(g_panel, IDC_AI_INPUT_EDIT), L"");
  }
}

void installInputEditSubclass() {
  if (!g_panel || g_originalInputEditProc) {
    return;
  }

  HWND input = ::GetDlgItem(g_panel, IDC_AI_INPUT_EDIT);
  if (!input) {
    return;
  }

  g_inputEdit = input;
  g_originalInputEditProc = reinterpret_cast<WNDPROC>(
      ::SetWindowLongPtrW(input, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(InputEditSubclassProc)));
}

void uninstallInputEditSubclass() {
  if (g_inputEdit && g_originalInputEditProc) {
    ::SetWindowLongPtrW(g_inputEdit, GWLP_WNDPROC,
                        reinterpret_cast<LONG_PTR>(g_originalInputEditProc));
  }

  g_inputEdit = nullptr;
  g_originalInputEditProc = nullptr;
}

void resizePanelControls() {
  if (!g_panel) {
    return;
  }

  RECT rc{};
  ::GetClientRect(g_panel, &rc);
  int width = rc.right - rc.left;
  int height = rc.bottom - rc.top;

  HWND providerCombo = ::GetDlgItem(g_panel, IDC_AI_PROVIDER_COMBO);
  HWND modelCombo = ::GetDlgItem(g_panel, IDC_AI_MODEL_COMBO);
  HWND signIn = ::GetDlgItem(g_panel, IDC_AI_COPILOT_SIGNIN_BUTTON);
  HWND fontPlus = ::GetDlgItem(g_panel, IDC_AI_FONT_INCREASE_BUTTON);
  HWND fontMinus = ::GetDlgItem(g_panel, IDC_AI_FONT_DECREASE_BUTTON);
  HWND settings = ::GetDlgItem(g_panel, IDC_AI_SETTINGS_BUTTON);
  HWND clear = ::GetDlgItem(g_panel, IDC_AI_CLEAR_BUTTON);
  HWND chat = ::GetDlgItem(g_panel, IDC_AI_CHAT_HISTORY);
  HWND input = ::GetDlgItem(g_panel, IDC_AI_INPUT_EDIT);
  HWND send = ::GetDlgItem(g_panel, IDC_AI_SEND_BUTTON);

  if (!providerCombo || !modelCombo || !signIn || !fontPlus || !fontMinus ||
      !settings || !clear || !chat || !input || !send) {
    return;
  }

  constexpr int margin = 5;
  constexpr int rowGap = 4;
  constexpr int inputHeight = 50;
  constexpr int buttonWidth = 50;

  auto controlHeight = [](HWND ctrl) {
    RECT wr{};
    ::GetWindowRect(ctrl, &wr);
    return wr.bottom - wr.top;
  };

  int rowY = margin;
  int providerH = controlHeight(providerCombo);
  int modelH = controlHeight(modelCombo);
  int signInH = controlHeight(signIn);
  int fontPlusH = controlHeight(fontPlus);
  int fontMinusH = controlHeight(fontMinus);
  int settingsH = controlHeight(settings);
  int clearH = controlHeight(clear);
  int rowHeight = std::max(std::max(providerH, modelH),
                           std::max(std::max(signInH, fontPlusH),
                                    std::max(fontMinusH, std::max(settingsH, clearH))));

  int signInW = 0;
  int fontW = 28;
  int settingsW = 64;
  int clearW = 52;
  int rightGap = 4;
  int rightGroupW =
      fontW + rightGap + fontW + rightGap + settingsW + rightGap + clearW;

  int rightX = width - margin - rightGroupW;
  if (rightX < margin + 80) {
    rightX = margin + 80;
  }

  int providerW = std::max(70, std::min(100, width / 5));
  int modelX = margin + providerW + rightGap;
  int modelW = rightX - modelX - rightGap;
  if (modelW < 110) {
    modelW = 110;
  }

  int buttonX = width - margin;
  int clearX = buttonX - clearW;
  int settingsX = clearX - rightGap - settingsW;
  int fontMinusX = settingsX - rightGap - fontW;
  int fontPlusX = fontMinusX - rightGap - fontW;

  ::MoveWindow(providerCombo, margin, rowY, providerW, rowHeight, TRUE);
  ::MoveWindow(modelCombo, modelX, rowY, modelW, rowHeight, TRUE);
  ::MoveWindow(signIn, width, rowY, signInW, rowHeight, TRUE);
  ::MoveWindow(fontPlus, fontPlusX, rowY, fontW, rowHeight, TRUE);
  ::MoveWindow(fontMinus, fontMinusX, rowY, fontW, rowHeight, TRUE);
  ::MoveWindow(settings, settingsX, rowY, settingsW, rowHeight, TRUE);
  ::MoveWindow(clear, clearX, rowY, clearW, rowHeight, TRUE);

  int topBarBottom = rowY + rowHeight;
  int chatTop = topBarBottom + rowGap;
  int inputTop = height - inputHeight - margin;
  int chatHeight = inputTop - chatTop - rowGap;
  if (chatHeight < 80) {
    chatHeight = 80;
  }

  ::MoveWindow(chat, margin, chatTop, width - 2 * margin, chatHeight, TRUE);
  ::MoveWindow(input, margin, inputTop, width - buttonWidth - 3 * margin,
               inputHeight, TRUE);
  ::MoveWindow(send, width - buttonWidth - margin, inputTop, buttonWidth,
               inputHeight, TRUE);
}

void updateChatFont() {
  if (!g_panel) {
    return;
  }

  if (g_chatFont) {
    ::DeleteObject(g_chatFont);
    g_chatFont = nullptr;
  }

  HDC dc = ::GetDC(g_panel);
  int logicalHeight = -MulDiv(g_fontSize, GetDeviceCaps(dc, LOGPIXELSY), 72);
  ::ReleaseDC(g_panel, dc);

  g_chatFont = ::CreateFontW(logicalHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                             FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

  HWND chat = ::GetDlgItem(g_panel, IDC_AI_CHAT_HISTORY);
  HWND input = ::GetDlgItem(g_panel, IDC_AI_INPUT_EDIT);
  if (chat) {
    ::SendMessageW(chat, WM_SETFONT, reinterpret_cast<WPARAM>(g_chatFont), TRUE);
  }
  if (input) {
    ::SendMessageW(input, WM_SETFONT, reinterpret_cast<WPARAM>(g_chatFont), TRUE);
  }
}

void applyLocalizedPanelText() {
  if (!g_panel) {
    return;
  }

  ::SetWindowTextW(g_panel, tr(TextId::PanelTitle));
  ::SetWindowTextW(::GetDlgItem(g_panel, IDC_AI_SETTINGS_BUTTON),
                   tr(TextId::PanelSettingsButton));
  ::SetWindowTextW(::GetDlgItem(g_panel, IDC_AI_CLEAR_BUTTON),
                   tr(TextId::PanelClearButton));
  ::SetWindowTextW(::GetDlgItem(g_panel, IDC_AI_SEND_BUTTON),
                   tr(TextId::PanelSendButton));
}

void populateLanguageCombo(HWND combo, UiLanguagePreference selected) {
  if (!combo) {
    return;
  }

  ::SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  std::array<UiLanguagePreference, 3> options = {
      UiLanguagePreference::FollowNotepad, UiLanguagePreference::English,
      UiLanguagePreference::Chinese};
  for (UiLanguagePreference option : options) {
    std::wstring text = getUiLanguageOptionText(option);
    ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
  }
  ::SendMessageW(combo, CB_SETCURSEL,
                 static_cast<WPARAM>(uiLanguagePreferenceToComboIndex(selected)),
                 0);
}

void applyLocalizedSettingsText(HWND hwnd) {
  ::SetWindowTextW(hwnd, tr(TextId::SettingsTitle));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_OPENAI_KEY_LABEL),
                   tr(TextId::SettingsOpenAIKeyLabel));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_GEMINI_KEY_LABEL),
                   tr(TextId::SettingsGeminiKeyLabel));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_CLAUDE_KEY_LABEL),
                   tr(TextId::SettingsClaudeKeyLabel));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_DEFAULT_PROVIDER_GROUP),
                   tr(TextId::SettingsDefaultProviderGroup));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_DEFAULT_PROVIDER_LABEL),
                   tr(TextId::SettingsDefaultProviderLabel));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_UI_LANGUAGE_LABEL),
                   tr(TextId::SettingsLanguageLabel));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_SEND_SHORTCUT_CHECK),
                   tr(TextId::SettingsCtrlEnter));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_API_KEYS_GROUP),
                   tr(TextId::SettingsApiKeysGroup));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_API_KEYS_OPENAI_HINT),
                   tr(TextId::SettingsApiKeysOpenAIHint));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_API_KEYS_GEMINI_HINT),
                   tr(TextId::SettingsApiKeysGeminiHint));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_API_KEYS_CLAUDE_HINT),
                   tr(TextId::SettingsApiKeysClaudeHint));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_PROMPT_PROFILE_GROUP),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u55AE\u8F2A\u63D0\u793A\u8A5E\u8A2D\u5B9A"
                       : L"Single-turn Prompt Profile");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_PROMPT_PRESET_LABEL),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u9810\u8A2D\u65B9\u6848\uFF1A"
                       : L"Preset:");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_RESPONSE_LANGUAGE_LABEL),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u56DE\u8986\u8A9E\u8A00\uFF1A"
                       : L"Response language:");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_ENCODING_LABEL),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u7DE8\u78BC\u5EFA\u8B70\uFF1A"
                       : L"Encoding suggestion:");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_DETAIL_LEVEL_LABEL),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u56DE\u8986\u8A73\u7D30\u5EA6\uFF1A"
                       : L"Response detail:");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_SCENARIO_GROUP),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u60C5\u5883\u6A21\u7D44"
                       : L"Scenario Modules");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_SCENARIO_EXPLAIN_CHECK),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u89E3\u91CB\u7A0B\u5F0F\u78BC"
                       : L"Explain code");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_SCENARIO_FIX_CHECK),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u4FEE\u6B63 bug"
                       : L"Fix bugs");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_SCENARIO_REFACTOR_CHECK),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u91CD\u69CB\u512A\u5316"
                       : L"Refactor");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_SCENARIO_TEST_CHECK),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u7522\u751F\u6E2C\u8A66"
                       : L"Generate tests");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_SCENARIO_DOC_CHECK),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u64B0\u5BEB\u6587\u4EF6"
                       : L"Write docs");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_OUTPUT_GROUP),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u8F38\u51FA\u898F\u5247"
                       : L"Output Rules");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_OUTPUT_CODE_ONLY_CHECK),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u9069\u5408\u6642\u50C5\u8F38\u51FA\u7A0B\u5F0F\u78BC"
                       : L"Code only when suitable");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_OUTPUT_PRESERVE_STYLE_CHECK),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u4FDD\u7559\u5C08\u6848\u98A8\u683C"
                       : L"Preserve project style");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_OUTPUT_RISKS_CHECK),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u63D0\u9192\u98A8\u96AA\u8207\u5047\u8A2D"
                       : L"Mention risks and assumptions");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_CUSTOM_PROMPT_GROUP),
                   g_uiLanguage == UiLanguage::Chinese
                       ? L"\u63D0\u793A\u8A5E\u9810\u89BD"
                       : L"Prompt Preview");
  ::SetWindowTextW(::GetDlgItem(hwnd, IDC_TEST_CONNECTION_BTN),
                   tr(TextId::SettingsTestConnection));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDOK), tr(TextId::SettingsOk));
  ::SetWindowTextW(::GetDlgItem(hwnd, IDCANCEL), tr(TextId::SettingsCancel));
}

void populateModelComboPlaceholder(HWND modelCombo, const std::wstring &text) {
  ::SendMessageW(modelCombo, CB_RESETCONTENT, 0, 0);
  ::SendMessageW(modelCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
  ::SendMessageW(modelCombo, CB_SETCURSEL, 0, 0);
  ::EnableWindow(modelCombo, FALSE);
  g_currentModel.clear();
}

void updateModelCombo() {
  if (!g_panel) {
    return;
  }

  HWND modelCombo = ::GetDlgItem(g_panel, IDC_AI_MODEL_COMBO);
  HWND signInButton = ::GetDlgItem(g_panel, IDC_AI_COPILOT_SIGNIN_BUTTON);
  if (!modelCombo) {
    return;
  }

  if (signInButton) {
    ::ShowWindow(signInButton, SW_HIDE);
  }

  const std::wstring apiKey = getProviderApiKey(g_currentProvider);
  if (apiKey.empty()) {
    populateModelComboPlaceholder(modelCombo, g_uiLanguage == UiLanguage::Chinese ? L"\u8ACB\u5148\u5728\u8A2D\u5B9A\u4E2D\u8A2D\u5B9A API Key" : L"Configure API key in Settings");
    return;
  }

  ModelListResponse response;
  switch (g_currentProvider) {
  case LLMProvider::OpenAI:
    response = LLMApiClient::listOpenAIModels(apiKey);
    break;
  case LLMProvider::Gemini:
    response = LLMApiClient::listGeminiModels(apiKey);
    break;
  case LLMProvider::Claude:
    response = LLMApiClient::listClaudeModels(apiKey);
    break;
  default:
    populateModelComboPlaceholder(modelCombo, g_uiLanguage == UiLanguage::Chinese ? L"\u4F9B\u61C9\u5546\u7121\u6CD5\u4F7F\u7528" : L"Provider unavailable");
    return;
  }

  if (!response.success || response.models.empty()) {
    populateModelComboPlaceholder(modelCombo, g_uiLanguage == UiLanguage::Chinese ? L"\u7121\u6CD5\u8F09\u5165\u6A21\u578B" : L"Unable to load models");
    if (!response.errorMessage.empty()) {
      addMessage(false, L"[Model Load Error] " + response.errorMessage);
      updateChatDisplay();
    }
    return;
  }

  ::EnableWindow(modelCombo, TRUE);
  ::SendMessageW(modelCombo, CB_RESETCONTENT, 0, 0);

  int selectedIndex = 0;
  for (size_t i = 0; i < response.models.size(); ++i) {
    const std::wstring &model = response.models[i];
    ::SendMessageW(modelCombo, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(model.c_str()));
    if (!g_currentModel.empty() && g_currentModel == model) {
      selectedIndex = static_cast<int>(i);
    }
  }

  ::SendMessageW(modelCombo, CB_SETCURSEL, static_cast<WPARAM>(selectedIndex), 0);
  g_currentModel = response.models[static_cast<size_t>(selectedIndex)];
  ::SendMessageW(modelCombo, CB_SETDROPPEDWIDTH, 250, 0);
}

UiLanguage resolvePromptUiLanguage(const AIAssistantConfig &config) {
  switch (config.uiLanguagePreference) {
  case UiLanguagePreference::English:
    return UiLanguage::English;
  case UiLanguagePreference::Chinese:
    return UiLanguage::Chinese;
  case UiLanguagePreference::FollowNotepad:
  default:
    return g_uiLanguage;
  }
}

std::wstring getPromptResponseLanguageInstruction(
    const AIAssistantConfig &config) {
  switch (config.responseLanguage) {
  case PromptResponseLanguage::TraditionalChinese:
    return L"Traditional Chinese";
  case PromptResponseLanguage::English:
    return L"English";
  case PromptResponseLanguage::FollowInterface:
  default:
    return resolvePromptUiLanguage(config) == UiLanguage::Chinese
               ? L"Traditional Chinese"
               : L"English";
  }
}

std::wstring getPromptEncodingInstruction(const AIAssistantConfig &config) {
  switch (config.encodingPreference) {
  case PromptEncodingPreference::UTF8:
    return L"UTF-8";
  case PromptEncodingPreference::UTF8Bom:
    return L"UTF-8 with BOM";
  case PromptEncodingPreference::Big5:
    return L"Big5";
  case PromptEncodingPreference::ANSI:
    return L"ANSI / local code page";
  case PromptEncodingPreference::CurrentDocument:
  default:
    break;
  }

  UINT_PTR bufferId = static_cast<UINT_PTR>(
      ::SendMessageW(g_nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0));
  const LRESULT encoding =
      ::SendMessageW(g_nppData._nppHandle, NPPM_GETBUFFERENCODING, bufferId, 0);
  switch (encoding) {
  case 1:
    return L"UTF-8 with BOM";
  case 4:
  case 5:
    return L"UTF-8";
  case 2:
    return L"UTF-16 BE with BOM";
  case 3:
    return L"UTF-16 LE with BOM";
  case 6:
    return L"UTF-16 BE";
  case 7:
    return L"UTF-16 LE";
  case 0:
  default: {
    HWND scintilla = getCurrentScintilla();
    if (scintilla) {
      int codePage = static_cast<int>(::SendMessage(scintilla, SCI_GETCODEPAGE, 0, 0));
      if (codePage == 950) {
        return L"Big5";
      }
    }
    return L"ANSI / current document encoding";
  }
  }
}

std::wstring getPromptLineEndingInstruction() {
  UINT_PTR bufferId = static_cast<UINT_PTR>(
      ::SendMessageW(g_nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0));
  const LRESULT format =
      ::SendMessageW(g_nppData._nppHandle, NPPM_GETBUFFERFORMAT, bufferId, 0);
  switch (format) {
  case 0:
    return L"CRLF";
  case 1:
    return L"CR";
  case 2:
    return L"LF";
  default:
    return L"Match the current document";
  }
}

std::wstring getPromptDetailInstruction(const AIAssistantConfig &config) {
  switch (config.detailLevel) {
  case PromptDetailLevel::Concise:
    return L"Keep the answer concise and practical.";
  case PromptDetailLevel::Detailed:
    return L"Provide a detailed answer with rationale and key tradeoffs.";
  case PromptDetailLevel::Standard:
  default:
    return L"Provide a balanced answer with the essential reasoning.";
  }
}

std::wstring buildEffectivePromptForConfig(const AIAssistantConfig &config,
                                           const std::wstring &userPrompt,
                                           bool forceCodeOnlyOutput = false) {
  std::wstringstream prompt;
  prompt << L"[Single-turn System]\n";
  prompt << L"- Treat this as a fully independent single-turn request. Do not rely on prior conversation.\n";
  prompt << L"- Work only from the information in this message.\n";
  prompt << L"- Reply language: "
         << getPromptResponseLanguageInstruction(config) << L".\n";
  prompt << L"- Preferred encoding for generated code/text: "
         << getPromptEncodingInstruction(config) << L".\n";
  prompt << L"- Preferred line ending style: " << getPromptLineEndingInstruction()
         << L".\n";
  prompt << L"- " << getPromptDetailInstruction(config) << L"\n";
  prompt << L"- The answer may be pasted back into an editor or code file.\n";

  if (config.scenarioFlags != 0) {
    prompt << L"\n[Scenario Modules]\n";
    if ((config.scenarioFlags & ScenarioExplainCode) != 0) {
      prompt << L"- Be ready to explain existing code, behavior, dependencies, or editor output.\n";
    }
    if ((config.scenarioFlags & ScenarioFixBugs) != 0) {
      prompt << L"- Prioritize identifying root causes and proposing the smallest correct fix.\n";
    }
    if ((config.scenarioFlags & ScenarioRefactor) != 0) {
      prompt << L"- Favor maintainable refactors that preserve behavior unless asked otherwise.\n";
    }
    if ((config.scenarioFlags & ScenarioGenerateTests) != 0) {
      prompt << L"- When useful, include or suggest focused automated tests.\n";
    }
    if ((config.scenarioFlags & ScenarioWriteDocs) != 0) {
      prompt << L"- When useful, produce clear developer-facing documentation or comments.\n";
    }
  }

  prompt << L"\n[Output Rules]\n";
  if (config.outputPreserveStyle) {
    prompt << L"- Preserve existing naming, formatting, and project style where possible.\n";
  }
  if (config.outputMentionRisks) {
    prompt << L"- Briefly call out important risks, assumptions, or edge cases.\n";
  }
  if (forceCodeOnlyOutput) {
    prompt << L"- Return only the final replacement code or text. Do not include markdown fences or explanation.\n";
  } else if (config.outputCodeOnly) {
    prompt << L"- When the task is code generation or code transformation, prefer directly usable output and keep explanation minimal.\n";
  }

  prompt << L"\n[User Request]\n";
  prompt << userPrompt;
  return prompt.str();
}

std::wstring buildEffectivePrompt(const std::wstring &userPrompt,
                                  bool forceCodeOnlyOutput = false) {
  return buildEffectivePromptForConfig(g_config, userPrompt, forceCodeOnlyOutput);
}

std::wstring getPromptPreviewUserRequest(const AIAssistantConfig &config) {
  const std::wstring lastRequest = trimWhitespace(g_lastPromptUserRequest);
  if (!lastRequest.empty()) {
    return lastRequest;
  }
  return resolvePromptUiLanguage(config) == UiLanguage::Chinese
             ? L"<\u9019\u88E1\u6703\u63D2\u5165\u5BE6\u969B\u9001\u51FA\u7684\u4F7F\u7528\u8005\u8ACB\u6C42\u6216\u9078\u53D6\u6587\u5B57>"
             : L"<The actual user request or selected text will be inserted here>";
}

void updatePromptPreviewInSettings(HWND hwnd, const AIAssistantConfig &config) {
  HWND previewEdit = ::GetDlgItem(hwnd, IDC_CUSTOM_PROMPT_EDIT);
  if (!previewEdit) {
    return;
  }
  std::wstring preview =
      buildEffectivePromptForConfig(config, getPromptPreviewUserRequest(config));
  ::SetWindowTextW(previewEdit, preview.c_str());
}
void initPanelControls() {
  loadConfig();
  refreshUiLanguage();
  g_currentProvider = sanitizeProvider(g_config.defaultProvider);

  HWND providerCombo = ::GetDlgItem(g_panel, IDC_AI_PROVIDER_COMBO);
  if (providerCombo) {
    ::SendMessageW(providerCombo, CB_RESETCONTENT, 0, 0);
    for (LLMProvider provider : kEnabledProviders) {
      std::wstring name = getProviderName(provider);
      ::SendMessageW(providerCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(name.c_str()));
    }
    ::SendMessageW(providerCombo, CB_SETCURSEL,
                   static_cast<WPARAM>(providerToComboIndex(g_currentProvider)), 0);
  }

  updateModelCombo();
  updateChatFont();
  applyLocalizedPanelText();
}

LLMResponse callCurrentProvider(const std::wstring &apiKey,
                                const std::wstring &prompt) {
  switch (g_currentProvider) {
  case LLMProvider::OpenAI:
    return LLMApiClient::callOpenAI(apiKey, prompt, g_currentModel);
  case LLMProvider::Gemini:
    return LLMApiClient::callGemini(apiKey, prompt, g_currentModel);
  case LLMProvider::Claude:
    return LLMApiClient::callClaude(apiKey, prompt, g_currentModel);
  default: {
    LLMResponse unsupported;
    unsupported.errorMessage = L"Unsupported provider";
    return unsupported;
  }
  }
}

std::wstring invokeProvider(const std::wstring &prompt) {
  if (g_currentProvider == LLMProvider::Copilot) {
    return L"[Notice] GitHub Copilot is currently paused in this build.";
  }

  const std::wstring apiKey = getProviderApiKey(g_currentProvider);

  if (apiKey.empty()) {
    return L"[Error] API key not configured for " + getProviderName(g_currentProvider) +
           L". Open Settings to configure it.";
  }

  if (g_currentModel.empty()) {
    return L"[Error] No model is available for " + getProviderName(g_currentProvider) +
           L". Check your API key and refresh the model list from Settings.";
  }

  LLMResponse response = callCurrentProvider(apiKey, prompt);
  if (response.success) {
    if (isInterruptedConversationMessage(response.content)) {
      LLMResponse retry = callCurrentProvider(apiKey, prompt);
      if (retry.success && !isInterruptedConversationMessage(retry.content)) {
        return retry.content;
      }
      return buildInterruptedConversationNotice();
    }
    return response.content;
  }

  if (isInterruptedConversationMessage(response.errorMessage)) {
    LLMResponse retry = callCurrentProvider(apiKey, prompt);
    if (retry.success && !isInterruptedConversationMessage(retry.content)) {
      return retry.content;
    }
    return buildInterruptedConversationNotice();
  }

  return L"[Error] " + getProviderName(g_currentProvider) +
         L" API call failed:\n" + response.errorMessage;
}

void sendPrompt(const std::wstring &prompt) {
  if (prompt.empty()) {
    return;
  }

  addMessage(true, prompt);
  updateChatDisplay();

  g_lastPromptUserRequest = prompt;
  std::wstring effectivePrompt = buildEffectivePrompt(prompt, false);
  std::wstring response = invokeProvider(effectivePrompt);
  addMessage(false, response);
  updateChatDisplay();
  clearInput();
}

INT_PTR CALLBACK SettingsDlgProc(HWND hwnd, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
  auto *config = reinterpret_cast<AIAssistantConfig *>(
      ::GetWindowLongPtrW(hwnd, DWLP_USER));

  switch (message) {
  case WM_INITDIALOG: {
    refreshUiLanguage();
    auto *incoming = reinterpret_cast<AIAssistantConfig *>(lParam);
    ::SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(incoming));

    ::SetWindowTextW(::GetDlgItem(hwnd, IDC_OPENAI_KEY_EDIT),
                     incoming->openAIKey.c_str());
    ::SetWindowTextW(::GetDlgItem(hwnd, IDC_GEMINI_KEY_EDIT),
                     incoming->geminiKey.c_str());
    ::SetWindowTextW(::GetDlgItem(hwnd, IDC_CLAUDE_KEY_EDIT),
                     incoming->claudeKey.c_str());

    HWND providerCombo = ::GetDlgItem(hwnd, IDC_DEFAULT_PROVIDER_COMBO);
    ::SendMessageW(providerCombo, CB_RESETCONTENT, 0, 0);
    for (LLMProvider provider : kEnabledProviders) {
      std::wstring providerName = getProviderName(provider);
      ::SendMessageW(providerCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(providerName.c_str()));
    }
    ::SendMessageW(providerCombo, CB_SETCURSEL,
                   static_cast<WPARAM>(
                       providerToComboIndex(sanitizeProvider(incoming->defaultProvider))),
                   0);
    populateLanguageCombo(::GetDlgItem(hwnd, IDC_UI_LANGUAGE_COMBO),
                          incoming->uiLanguagePreference);
    syncPromptControlsFromConfig(hwnd, *incoming);

    const wchar_t maskChar = L'*';
    ::SendMessageW(::GetDlgItem(hwnd, IDC_OPENAI_KEY_EDIT), EM_SETPASSWORDCHAR,
                   maskChar, 0);
    ::SendMessageW(::GetDlgItem(hwnd, IDC_GEMINI_KEY_EDIT), EM_SETPASSWORDCHAR,
                   maskChar, 0);
    ::SendMessageW(::GetDlgItem(hwnd, IDC_CLAUDE_KEY_EDIT), EM_SETPASSWORDCHAR,
                   maskChar, 0);
    ::SendMessageW(::GetDlgItem(hwnd, IDC_SEND_SHORTCUT_CHECK), BM_SETCHECK,
                   incoming->requireCtrlEnterToSend ? BST_CHECKED : BST_UNCHECKED,
                   0);
    applyLocalizedSettingsText(hwnd);
    updatePromptPreviewInSettings(hwnd, *incoming);
    return TRUE;
  }

  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDC_PROMPT_PRESET_COMBO:
      if (HIWORD(wParam) == CBN_SELCHANGE && config) {
        int presetSelection = static_cast<int>(::SendMessageW(
            ::GetDlgItem(hwnd, IDC_PROMPT_PRESET_COMBO), CB_GETCURSEL, 0, 0));
        applyPromptPresetToConfig(*config, comboIndexToPromptPreset(presetSelection));
        syncPromptControlsFromConfig(hwnd, *config);
        updatePromptPreviewInSettings(hwnd, *config);
        return TRUE;
      }
      break;

    case IDC_RESPONSE_LANGUAGE_COMBO:
    case IDC_ENCODING_COMBO:
    case IDC_DETAIL_LEVEL_COMBO:
    case IDC_UI_LANGUAGE_COMBO:
      if (HIWORD(wParam) == CBN_SELCHANGE && config) {
        capturePromptSettingsFromDialog(hwnd, *config);
        updatePromptPreviewInSettings(hwnd, *config);
        return TRUE;
      }
      break;

    case IDC_SCENARIO_EXPLAIN_CHECK:
    case IDC_SCENARIO_FIX_CHECK:
    case IDC_SCENARIO_REFACTOR_CHECK:
    case IDC_SCENARIO_TEST_CHECK:
    case IDC_SCENARIO_DOC_CHECK:
    case IDC_OUTPUT_CODE_ONLY_CHECK:
    case IDC_OUTPUT_PRESERVE_STYLE_CHECK:
    case IDC_OUTPUT_RISKS_CHECK:
      if (HIWORD(wParam) == BN_CLICKED && config) {
        capturePromptSettingsFromDialog(hwnd, *config);
        updatePromptPreviewInSettings(hwnd, *config);
        return TRUE;
      }
      break;

    case IDC_TEST_CONNECTION_BTN: {
      if (!config) {
        return TRUE;
      }

      wchar_t text[512]{};
      ::GetWindowTextW(::GetDlgItem(hwnd, IDC_OPENAI_KEY_EDIT), text, 512);
      config->openAIKey = trimWhitespace(text);
      ::GetWindowTextW(::GetDlgItem(hwnd, IDC_GEMINI_KEY_EDIT), text, 512);
      config->geminiKey = trimWhitespace(text);
      ::GetWindowTextW(::GetDlgItem(hwnd, IDC_CLAUDE_KEY_EDIT), text, 512);
      config->claudeKey = trimWhitespace(text);
      capturePromptSettingsFromDialog(hwnd, *config);

      int providerSelection = static_cast<int>(::SendMessageW(
          ::GetDlgItem(hwnd, IDC_DEFAULT_PROVIDER_COMBO), CB_GETCURSEL, 0, 0));
      LLMProvider provider = comboIndexToProvider(providerSelection);

      std::wstring apiKey;
      std::wstring providerName;
      switch (provider) {
      case LLMProvider::OpenAI:
        apiKey = config->openAIKey;
        providerName = L"OpenAI";
        break;
      case LLMProvider::Gemini:
        apiKey = config->geminiKey;
        providerName = L"Gemini";
        break;
      case LLMProvider::Claude:
        apiKey = config->claudeKey;
        providerName = L"Claude";
        break;
      default:
        return TRUE;
      }

      if (apiKey.empty()) {
        const std::wstring warningText =
            g_uiLanguage == UiLanguage::Chinese
                ? L"\u5C1A\u672A\u8A2D\u5B9A " + providerName + L" API Key\u3002"
                : L"No API key configured for " + providerName + L".";
        ::MessageBoxW(hwnd, warningText.c_str(), tr(TextId::SettingsTitle),
                      MB_OK | MB_ICONWARNING);
        return TRUE;
      }

      HCURSOR oldCursor = ::SetCursor(::LoadCursorW(nullptr, IDC_WAIT));
      ModelListResponse testResponse;
      if (provider == LLMProvider::OpenAI) {
        testResponse = LLMApiClient::listOpenAIModels(apiKey);
      } else if (provider == LLMProvider::Gemini) {
        testResponse = LLMApiClient::listGeminiModels(apiKey);
      } else if (provider == LLMProvider::Claude) {
        testResponse = LLMApiClient::listClaudeModels(apiKey);
      }
      ::SetCursor(oldCursor);

      std::wstring messageText;
      if (testResponse.success) {
        messageText = g_uiLanguage == UiLanguage::Chinese
                          ? L"\u8207 " + providerName + L" \u9023\u7DDA\u6210\u529F\u3002"
                          : L"Connection to " + providerName + L" succeeded.";
        if (!testResponse.models.empty()) {
          messageText += L"\n\n";
          messageText +=
              g_uiLanguage == UiLanguage::Chinese
                  ? L"\u5075\u6E2C\u5230\u6A21\u578B\u6578\uFF1A" +
                        std::to_wstring(testResponse.models.size())
                  : L"Detected models: " +
                        std::to_wstring(testResponse.models.size());
          messageText +=
              g_uiLanguage == UiLanguage::Chinese
                  ? L"\n\u9996\u500B\u6A21\u578B\uFF1A" + testResponse.models.front()
                  : L"\nFirst model: " + testResponse.models.front();
        }
      } else {
        messageText = g_uiLanguage == UiLanguage::Chinese
                          ? L"\u8207 " + providerName + L" \u9023\u7DDA\u5931\u6557\u3002\n\n" + testResponse.errorMessage
                          : L"Connection to " + providerName +
                                L" failed.\n\n" + testResponse.errorMessage;
      }
      ::MessageBoxW(hwnd, messageText.c_str(), tr(TextId::SettingsTitle),
                    MB_OK | (testResponse.success ? MB_ICONINFORMATION : MB_ICONERROR));
      return TRUE;
    }

    case IDOK: {
      if (!config) {
        ::EndDialog(hwnd, IDOK);
        return TRUE;
      }

      wchar_t text[512]{};
      ::GetWindowTextW(::GetDlgItem(hwnd, IDC_OPENAI_KEY_EDIT), text, 512);
      config->openAIKey = trimWhitespace(text);
      ::GetWindowTextW(::GetDlgItem(hwnd, IDC_GEMINI_KEY_EDIT), text, 512);
      config->geminiKey = trimWhitespace(text);
      ::GetWindowTextW(::GetDlgItem(hwnd, IDC_CLAUDE_KEY_EDIT), text, 512);
      config->claudeKey = trimWhitespace(text);
      capturePromptSettingsFromDialog(hwnd, *config);

      ::EndDialog(hwnd, IDOK);
      return TRUE;
    }

    case IDCANCEL:
      ::EndDialog(hwnd, IDCANCEL);
      return TRUE;
    }
    break;
  }

  return FALSE;
}

void openSettingsDialog() {
  AIAssistantConfig edited = g_config;
  if (::DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_AIASSISTANT_SETTINGS),
                        g_nppData._nppHandle, SettingsDlgProc,
                        reinterpret_cast<LPARAM>(&edited)) == IDOK) {
    saveConfig(edited);
    refreshUiLanguage();
    commandMenuInit();
    g_currentProvider = sanitizeProvider(g_config.defaultProvider);
    if (g_panel) {
      HWND providerCombo = ::GetDlgItem(g_panel, IDC_AI_PROVIDER_COMBO);
      if (providerCombo) {
        ::SendMessageW(providerCombo, CB_SETCURSEL,
                       static_cast<WPARAM>(providerToComboIndex(g_currentProvider)), 0);
      }
      applyLocalizedPanelText();
    }
    updateModelCombo();
    updateChatDisplay();
  }
}

void completeCopilotAuth(bool success) {
  ::KillTimer(g_panel, kCopilotPollTimerId);
  g_copilotAuthInProgress = false;

  if (success) {
    saveCopilotToken();
    addMessage(false, L"Successfully signed in to GitHub Copilot.");
  } else {
    std::wstring message =
        L"[Error] Failed to complete GitHub Copilot sign-in.";
    std::wstring debugInfo = LLMApiClient::getLastCopilotAuthDebug();
    if (!debugInfo.empty()) {
      message += L"\n\nDetails:\n" + debugInfo;
    }
    addMessage(false, message);
  }

  updateModelCombo();
  updateChatDisplay();
}

void pollCopilotAuth() {
  if (!g_copilotAuthInProgress) {
    ::KillTimer(g_panel, kCopilotPollTimerId);
    return;
  }

  CopilotTokens tokens;
  int result =
      LLMApiClient::pollCopilotAccessToken(g_copilotDeviceCode.deviceCode, tokens);
  if (result == 1 && !tokens.oauthToken.empty()) {
    g_copilotTokens = tokens;
    completeCopilotAuth(true);
    return;
  }

  if (result == -1) {
    completeCopilotAuth(false);
    return;
  }

  std::wstring debugInfo = LLMApiClient::getLastCopilotAuthDebug();
  if (debugInfo.find(L"slow_down") != std::wstring::npos) {
    g_copilotPollIntervalMs += 5000;
    ::KillTimer(g_panel, kCopilotPollTimerId);
    ::SetTimer(g_panel, kCopilotPollTimerId, g_copilotPollIntervalMs, nullptr);
  }
  if (!debugInfo.empty() && debugInfo != g_lastCopilotDebug) {
    addMessage(false, L"Last response:\n" + debugInfo);
    g_lastCopilotDebug = debugInfo;
    updateChatDisplay();
  }

  DWORD tick = ::GetTickCount();
  if (tick - g_copilotLastPendingTick >= 30000) {
    addMessage(false, L"Still waiting for GitHub authorization...");
    g_copilotLastPendingTick = tick;
    updateChatDisplay();
  }
}

void beginCopilotSignIn() {
  if (g_copilotAuthInProgress) {
    g_copilotAuthInProgress = false;
    ::KillTimer(g_panel, kCopilotPollTimerId);
    updateModelCombo();
    addMessage(false, L"GitHub Copilot sign-in cancelled.");
    updateChatDisplay();
    return;
  }

  g_copilotDeviceCode = LLMApiClient::initiateCopilotDeviceFlow();
  if (g_copilotDeviceCode.userCode.empty() ||
      g_copilotDeviceCode.deviceCode.empty()) {
    std::wstring message =
        L"[Error] Failed to initiate GitHub Copilot sign-in.";
    std::wstring debugInfo = LLMApiClient::getLastCopilotAuthDebug();
    if (!debugInfo.empty()) {
      message += L"\n\nDetails:\n" + debugInfo;
    }
    addMessage(false, message);
    updateChatDisplay();
    return;
  }

  g_copilotAuthInProgress = true;
  g_copilotLastPendingTick = ::GetTickCount();
  g_lastCopilotDebug.clear();

  std::wstring authText =
      L"To sign in to GitHub Copilot:\n\n1. Go to: " +
      g_copilotDeviceCode.verificationUri + L"\n2. Enter code: " +
      g_copilotDeviceCode.userCode + L"\n\nWaiting for authorization...";
  addMessage(false, authText);
  updateChatDisplay();
  updateModelCombo();

  ::ShellExecuteW(nullptr, L"open", g_copilotDeviceCode.verificationUri.c_str(),
                  nullptr, nullptr, SW_SHOWNORMAL);

  DWORD interval = static_cast<DWORD>(g_copilotDeviceCode.interval) * 1000;
  if (interval < kDefaultCopilotPollMs) {
    interval = kDefaultCopilotPollMs;
  }
  g_copilotPollIntervalMs = interval;
  ::SetTimer(g_panel, kCopilotPollTimerId, g_copilotPollIntervalMs, nullptr);
}

INT_PTR CALLBACK PanelDlgProc(HWND hwnd, UINT message, WPARAM wParam,
                              LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);

  switch (message) {
  case WM_INITDIALOG:
    g_panel = hwnd;
    initPanelControls();
    installInputEditSubclass();
    resizePanelControls();
    addMessage(false, tr(TextId::WelcomeMessage));
    updateChatDisplay();
    return TRUE;

  case WM_SIZE:
    resizePanelControls();
    return TRUE;

  case WM_TIMER:
    if (wParam == kCopilotPollTimerId) {
      pollCopilotAuth();
      return TRUE;
    }
    break;

  case WM_NOTIFY: {
    auto *nmhdr = reinterpret_cast<LPNMHDR>(lParam);
    if (nmhdr && nmhdr->code == DMN_CLOSE) {
      g_panelVisible = false;
      return TRUE;
    }
    break;
  }

  case WM_DESTROY:
    uninstallInputEditSubclass();
    if (g_panel == hwnd) {
      g_panel = nullptr;
    }
    return TRUE;

  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDC_AI_SEND_BUTTON: {
      wchar_t buffer[4096]{};
      ::GetWindowTextW(::GetDlgItem(hwnd, IDC_AI_INPUT_EDIT), buffer, 4096);
      sendPrompt(buffer);
      return TRUE;
    }

    case IDC_AI_CLEAR_BUTTON:
      g_chatHistory.clear();
      updateChatDisplay();
      return TRUE;

    case IDC_AI_SETTINGS_BUTTON:
      openSettingsDialog();
      return TRUE;

    case IDC_AI_FONT_INCREASE_BUTTON:
      if (g_fontSize < kMaxFontSize) {
        ++g_fontSize;
        updateChatFont();
      }
      return TRUE;

    case IDC_AI_FONT_DECREASE_BUTTON:
      if (g_fontSize > kMinFontSize) {
        --g_fontSize;
        updateChatFont();
      }
      return TRUE;

    case IDC_AI_PROVIDER_COMBO:
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        int selection = static_cast<int>(
            ::SendMessageW(reinterpret_cast<HWND>(lParam), CB_GETCURSEL, 0, 0));
        if (selection >= 0 &&
            selection < static_cast<int>(kEnabledProviders.size())) {
          g_currentProvider = comboIndexToProvider(selection);
          updateModelCombo();
          updateChatDisplay();
        }
      }
      return TRUE;

    case IDC_AI_MODEL_COMBO:
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        wchar_t model[128]{};
        HWND combo = reinterpret_cast<HWND>(lParam);
        int selection = static_cast<int>(::SendMessageW(combo, CB_GETCURSEL, 0, 0));
        if (selection >= 0) {
          ::SendMessageW(combo, CB_GETLBTEXT, selection,
                         reinterpret_cast<LPARAM>(model));
          g_currentModel = model;
        }
      }
      return TRUE;
    }
    break;
  }

  return FALSE;
}

bool ensurePanel() {
  if (g_panel) {
    return true;
  }

  HWND panel = ::CreateDialogParamW(g_hInst, MAKEINTRESOURCEW(IDD_AIASSISTANT_PANEL),
                                    g_nppData._nppHandle, PanelDlgProc, 0);
  if (!panel) {
    ::MessageBoxW(g_nppData._nppHandle, L"Failed to create AI Assistant panel.",
                  L"NppAIAssistant", MB_OK | MB_ICONERROR);
    return false;
  }

  ::SendMessageW(g_nppData._nppHandle, NPPM_MODELESSDIALOG, MODELESSDIALOGADD,
                 reinterpret_cast<LPARAM>(panel));

  if (!g_panelRegistered) {
    tTbData dockData{};
    dockData.hClient = panel;
    dockData.pszName = tr(TextId::PanelTitle);
    dockData.dlgID = g_funcItems[0]._cmdID;
    dockData.uMask = DWS_DF_CONT_RIGHT;
    dockData.pszModuleName = g_moduleFileName.c_str();
    ::SendMessageW(g_nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0,
                   reinterpret_cast<LPARAM>(&dockData));
    g_panelRegistered = true;
  }

  return true;
}

void showPanel() {
  if (!ensurePanel()) {
    return;
  }

  ::SendMessageW(g_nppData._nppHandle, NPPM_DMMSHOW, 0,
                 reinterpret_cast<LPARAM>(g_panel));
  g_panelVisible = true;
}

void togglePanel() {
  if (!ensurePanel()) {
    return;
  }

  if (g_panelVisible) {
    ::SendMessageW(g_nppData._nppHandle, NPPM_DMMHIDE, 0,
                   reinterpret_cast<LPARAM>(g_panel));
    g_panelVisible = false;
  } else {
    showPanel();
  }
}

void queuePrompt(const std::wstring &prompt) {
  showPanel();
  if (!g_panel || prompt.empty()) {
    return;
  }

  ::SetWindowTextW(::GetDlgItem(g_panel, IDC_AI_INPUT_EDIT), prompt.c_str());
  sendPrompt(prompt);
}

void runSelectionCommand(const wchar_t *prefix, SelectionAction action) {
  SelectionContext context = getCurrentSelectionContext();
  if (!context.scintilla || context.text.empty()) {
    ::MessageBoxW(g_nppData._nppHandle,
                  tr(TextId::SelectTextWarning),
                  L"NppAIAssistant", MB_OK | MB_ICONINFORMATION);
    return;
  }

  std::wstring prompt = std::wstring(prefix) + L"\n\n" + context.text;
  showPanel();
  addMessage(true, prompt);
  updateChatDisplay();

  std::wstring effectivePrompt =
      buildEffectivePrompt(prompt, action == SelectionAction::ReplaceSelection);
  std::wstring response = invokeProvider(effectivePrompt);
  addMessage(false, response);
  updateChatDisplay();

  if (action == SelectionAction::ReplaceSelection &&
      response.rfind(L"[Error]", 0) != 0 && response.rfind(L"[Notice]", 0) != 0) {
    if (!replaceSelectionText(context, response)) {
      addMessage(false,
                 L"[Error] Failed to write the AI result back to the editor.");
      updateChatDisplay();
    }
  }

  clearInput();
}

void showAiContextMenu(HWND scintilla, LPARAM lParam) {
  if (!scintilla || getSelectionText(scintilla).empty()) {
    return;
  }

  refreshUiLanguage();

  POINT pt{};
  if (lParam == static_cast<LPARAM>(-1)) {
    ::GetCursorPos(&pt);
  } else {
    pt.x = static_cast<short>(LOWORD(lParam));
    pt.y = static_cast<short>(HIWORD(lParam));
  }

  HMENU menu = ::CreatePopupMenu();
  if (!menu) {
    return;
  }

  ::AppendMenuW(menu, MF_STRING, kAiContextExplain, tr(TextId::ContextExplain));
  ::AppendMenuW(menu, MF_STRING, kAiContextRefactor,
                tr(TextId::ContextRefactor));
  ::AppendMenuW(menu, MF_STRING, kAiContextComments,
                tr(TextId::ContextComments));
  ::AppendMenuW(menu, MF_STRING, kAiContextFix, tr(TextId::ContextFix));

  const UINT selected = ::TrackPopupMenu(
      menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0,
      scintilla, nullptr);
  ::DestroyMenu(menu);

  switch (selected) {
  case kAiContextExplain:
    cmdExplainSelection();
    break;
  case kAiContextRefactor:
    cmdRefactorSelection();
    break;
  case kAiContextComments:
    cmdAddComments();
    break;
  case kAiContextFix:
    cmdFixCode();
    break;
  default:
    break;
  }
}

void installScintillaSubclass(HWND scintilla, size_t index) {
  if (!scintilla || index >= g_scintillaWindows.size() ||
      g_originalSciWndProc[index] != nullptr) {
    return;
  }

  g_scintillaWindows[index] = scintilla;
  g_originalSciWndProc[index] = reinterpret_cast<WNDPROC>(
      ::SetWindowLongPtrW(scintilla, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(ScintillaSubclassProc)));
}

void installScintillaHooks() {
  installScintillaSubclass(g_nppData._scintillaMainHandle, 0);
  installScintillaSubclass(g_nppData._scintillaSecondHandle, 1);
}

void uninstallScintillaHooks() {
  for (size_t i = 0; i < g_scintillaWindows.size(); ++i) {
    if (g_scintillaWindows[i] && g_originalSciWndProc[i]) {
      ::SetWindowLongPtrW(g_scintillaWindows[i], GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(g_originalSciWndProc[i]));
      g_originalSciWndProc[i] = nullptr;
      g_scintillaWindows[i] = nullptr;
    }
  }
}

LRESULT CALLBACK ScintillaSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                       LPARAM lParam) {
  size_t index = 0;
  while (index < g_scintillaWindows.size() && g_scintillaWindows[index] != hwnd) {
    ++index;
  }

  WNDPROC original =
      index < g_originalSciWndProc.size() ? g_originalSciWndProc[index] : nullptr;
  if (!original) {
    return ::DefWindowProcW(hwnd, message, wParam, lParam);
  }

  if (message == WM_CONTEXTMENU && !getSelectionText(hwnd).empty()) {
    showAiContextMenu(hwnd, lParam);
    return 0;
  }

  return ::CallWindowProcW(original, hwnd, message, wParam, lParam);
}

LRESULT CALLBACK InputEditSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                       LPARAM lParam) {
  if (!g_originalInputEditProc) {
    return ::DefWindowProcW(hwnd, message, wParam, lParam);
  }

  if ((message == WM_KEYDOWN || message == WM_CHAR) && wParam == VK_RETURN) {
    const bool shiftPressed = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool ctrlPressed = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shouldSend = g_config.requireCtrlEnterToSend ? ctrlPressed
                                                            : !shiftPressed;
    if (shouldSend) {
      ::PostMessageW(::GetParent(hwnd), WM_COMMAND,
                     MAKEWPARAM(IDC_AI_SEND_BUTTON, BN_CLICKED),
                     reinterpret_cast<LPARAM>(::GetDlgItem(::GetParent(hwnd),
                                                           IDC_AI_SEND_BUTTON)));
      return 0;
    }
  }

  return ::CallWindowProcW(g_originalInputEditProc, hwnd, message, wParam, lParam);
}

void cmdTogglePanel() { togglePanel(); }
void cmdExplainSelection() {
  runSelectionCommand(L"Explain this code:", SelectionAction::Explain);
}
void cmdRefactorSelection() {
  runSelectionCommand(
      L"Refactor this code for better readability and performance:",
      SelectionAction::ReplaceSelection);
}
void cmdAddComments() {
  runSelectionCommand(L"Add detailed comments to this code:",
                      SelectionAction::ReplaceSelection);
}
void cmdFixCode() {
  runSelectionCommand(L"Find and fix bugs in this code:",
                      SelectionAction::ReplaceSelection);
}
void cmdSettings() {
  loadConfig();
  openSettingsDialog();
}

void commandMenuInit() {
  setCommand(0, tr(TextId::PluginOpenPanel), cmdTogglePanel);
  setCommand(1, tr(TextId::PluginExplain), cmdExplainSelection);
  setCommand(2, tr(TextId::PluginRefactor), cmdRefactorSelection);
  setCommand(3, tr(TextId::PluginComments), cmdAddComments);
  setCommand(4, tr(TextId::PluginFix), cmdFixCode);
  setCommand(5, tr(TextId::PluginSettings), cmdSettings);
}

} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
  UNREFERENCED_PARAMETER(reserved);
  if (reason == DLL_PROCESS_ATTACH) {
    g_hInst = hModule;
    wchar_t path[MAX_PATH]{};
    ::GetModuleFileNameW(hModule, path, MAX_PATH);
    g_moduleFileName = path;
  } else if (reason == DLL_PROCESS_DETACH) {
    uninstallInputEditSubclass();
    uninstallScintillaHooks();
  }
  return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData nppData) {
  g_nppData = nppData;
  loadConfig();
  refreshUiLanguage();
  commandMenuInit();
  installScintillaHooks();
}

extern "C" __declspec(dllexport) const wchar_t *getName() { return kPluginName; }

extern "C" __declspec(dllexport) FuncItem *getFuncsArray(int *nbF) {
  *nbF = static_cast<int>(kMenuCount);
  return g_funcItems;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode) {
  if (!notifyCode) {
    return;
  }

  switch (notifyCode->nmhdr.code) {
  case NPPN_READY:
    refreshUiLanguage();
    commandMenuInit();
    if (g_panel) {
      applyLocalizedPanelText();
      updateChatDisplay();
    }
    break;
  default:
    break;
  }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT message, WPARAM wParam,
                                                     LPARAM lParam) {
  UNREFERENCED_PARAMETER(message);
  UNREFERENCED_PARAMETER(wParam);
  UNREFERENCED_PARAMETER(lParam);
  return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }























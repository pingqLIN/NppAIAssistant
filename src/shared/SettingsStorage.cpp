// This file is part of Notepad++ project
// Copyright (C)2025 Don HO <don.h@free.fr>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "SettingsStorage.h"

#include <ShlObj.h>
#include <vector>

namespace {

constexpr wchar_t kSettingsSection[] = L"Settings";
constexpr wchar_t kSchemaVersionKey[] = L"schema_version";
constexpr wchar_t kSettingsFileName[] = L"NppAIAssistant.ini";
constexpr wchar_t kMissingSentinel[] = L"__NPPAI_MISSING__";
constexpr DWORD kSettingsBufferSize = 65535;
thread_local bool g_writeBatchActive = false;
thread_local bool g_writeBatchSucceeded = true;

void recordWriteResult(bool succeeded) {
  if (g_writeBatchActive && !succeeded) g_writeBatchSucceeded = false;
}

std::wstring getAppDataConfigDirectory() {
  wchar_t appDataPath[MAX_PATH] = {};
  if (FAILED(
          SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
    return L"";
  }

  std::wstring notepadPath = std::wstring(appDataPath) + L"\\Notepad++";
  std::wstring pluginsPath = notepadPath + L"\\plugins";
  std::wstring configPath = pluginsPath + L"\\config";

  CreateDirectory(notepadPath.c_str(), nullptr);
  CreateDirectory(pluginsPath.c_str(), nullptr);
  CreateDirectory(configPath.c_str(), nullptr);
  return configPath;
}

std::wstring readRawValue(const std::wstring &keyName) {
  const std::wstring filePath = SettingsStorage::getConfigFilePath();
  if (filePath.empty()) {
    return L"";
  }

  std::vector<wchar_t> buffer(kSettingsBufferSize, L'\0');
  const DWORD copied = GetPrivateProfileStringW(
      kSettingsSection, keyName.c_str(), kMissingSentinel, buffer.data(),
      static_cast<DWORD>(buffer.size()), filePath.c_str());
  const std::wstring value(buffer.data(), copied);
  return value == kMissingSentinel ? L"" : value;
}

} // namespace

std::wstring SettingsStorage::getConfigFilePath() {
  const std::wstring configDirectory = getAppDataConfigDirectory();
  if (configDirectory.empty()) {
    return L"";
  }

  return configDirectory + L"\\" + kSettingsFileName;
}

int SettingsStorage::loadSchemaVersion() {
  const std::wstring rawValue = readRawValue(kSchemaVersionKey);
  if (rawValue.empty()) {
    return 0;
  }

  try {
    return std::stoi(rawValue);
  } catch (...) {
    return 0;
  }
}

bool SettingsStorage::saveSchemaVersion(int version) {
  return saveString(kSchemaVersionKey, std::to_wstring(version));
}

void SettingsStorage::beginWriteBatch() {
  g_writeBatchActive = true;
  g_writeBatchSucceeded = true;
}

bool SettingsStorage::endWriteBatch() {
  const bool succeeded = !g_writeBatchActive || g_writeBatchSucceeded;
  g_writeBatchActive = false;
  g_writeBatchSucceeded = true;
  return succeeded;
}

std::wstring SettingsStorage::loadString(const std::wstring &keyName) {
  return unescapeValue(readRawValue(keyName));
}

bool SettingsStorage::saveString(const std::wstring &keyName,
                                 const std::wstring &value) {
  const std::wstring filePath = getConfigFilePath();
  if (filePath.empty()) {
    recordWriteResult(false);
    return false;
  }

  const std::wstring escaped = escapeValue(value);
  const bool succeeded =
      WritePrivateProfileStringW(kSettingsSection, keyName.c_str(),
                                 escaped.c_str(), filePath.c_str()) != 0;
  recordWriteResult(succeeded);
  return succeeded;
}

std::wstring SettingsStorage::escapeValue(const std::wstring &value) {
  std::wstring escaped;
  escaped.reserve(value.size() * 2);

  for (wchar_t ch : value) {
    switch (ch) {
    case L'\\':
      escaped += L"\\\\";
      break;
    case L'\n':
      escaped += L"\\n";
      break;
    case L'\r':
      escaped += L"\\r";
      break;
    case L'\t':
      escaped += L"\\t";
      break;
    default:
      escaped += ch;
      break;
    }
  }

  return escaped;
}

std::wstring SettingsStorage::unescapeValue(const std::wstring &value) {
  std::wstring unescaped;
  unescaped.reserve(value.size());

  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] != L'\\' || i + 1 >= value.size()) {
      unescaped += value[i];
      continue;
    }

    const wchar_t escapedChar = value[++i];
    switch (escapedChar) {
    case L'\\':
      unescaped += L'\\';
      break;
    case L'n':
      unescaped += L'\n';
      break;
    case L'r':
      unescaped += L'\r';
      break;
    case L't':
      unescaped += L'\t';
      break;
    default:
      unescaped += L'\\';
      unescaped += escapedChar;
      break;
    }
  }

  return unescaped;
}

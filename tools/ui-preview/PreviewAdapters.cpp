#include "HttpClient.h"
#include "SecureStorage.h"
#include "SettingsStorage.h"

#include <map>
#include <mutex>
#include <wincrypt.h>

namespace {
std::map<std::wstring, std::wstring> preferences{
    {L"ui_language_preference", L"2"}};
int schema = 0;
#ifdef NPPAI_PREVIEW_ENABLE_NETWORK
std::mutex keyMutex;
std::map<std::wstring, std::vector<BYTE>> sessionKeys;
#else
HttpResponse blocked() {
  HttpResponse response;
  response.errorMessage = L"UI preview: network requests are disabled.";
  return response;
}
#endif
}

std::wstring SettingsStorage::getConfigFilePath() { return L""; }
int SettingsStorage::loadSchemaVersion() { return schema; }
bool SettingsStorage::saveSchemaVersion(int value) { schema = value; return true; }
void SettingsStorage::beginWriteBatch() {}
bool SettingsStorage::endWriteBatch() { return true; }
std::wstring SettingsStorage::loadString(const std::wstring &name) {
  const auto item = preferences.find(name);
  return item == preferences.end() ? L"" : item->second;
}
bool SettingsStorage::saveString(const std::wstring &name, const std::wstring &value) {
  preferences[name] = value;
  return true;
}

#ifdef NPPAI_PREVIEW_ENABLE_NETWORK
// The connected preview retains DPAPI-protected blobs only in this process.
// Production credential paths and legacy migration remain inaccessible.
bool SecureStorage::saveApiKey(const std::wstring &name, const std::wstring &value) {
  std::lock_guard<std::mutex> lock(keyMutex);
  if (value.empty()) { sessionKeys.erase(name); return true; }
  DATA_BLOB input{static_cast<DWORD>(value.size() * sizeof(wchar_t)),
                  reinterpret_cast<BYTE *>(const_cast<wchar_t *>(value.data()))};
  DATA_BLOB output{};
  if (!::CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr,
                         CRYPTPROTECT_UI_FORBIDDEN, &output)) return false;
  sessionKeys[name] = std::vector<BYTE>(output.pbData, output.pbData + output.cbData);
  ::LocalFree(output.pbData);
  return true;
}
std::wstring SecureStorage::loadApiKey(const std::wstring &name) {
  std::lock_guard<std::mutex> lock(keyMutex);
  const auto item = sessionKeys.find(name);
  if (item == sessionKeys.end()) return L"";
  DATA_BLOB input{static_cast<DWORD>(item->second.size()), item->second.data()};
  DATA_BLOB output{};
  if (!::CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                           CRYPTPROTECT_UI_FORBIDDEN, &output)) return L"";
  std::wstring value;
  if (output.cbData % sizeof(wchar_t) == 0)
    value.assign(reinterpret_cast<wchar_t *>(output.pbData), output.cbData / sizeof(wchar_t));
  ::SecureZeroMemory(output.pbData, output.cbData);
  ::LocalFree(output.pbData);
  return value;
}
bool SecureStorage::deleteApiKey(const std::wstring &name) {
  std::lock_guard<std::mutex> lock(keyMutex);
  sessionKeys.erase(name);
  return true;
}
bool SecureStorage::hasApiKey(const std::wstring &name) {
  std::lock_guard<std::mutex> lock(keyMutex);
  return sessionKeys.contains(name);
}
#else
bool SecureStorage::saveApiKey(const std::wstring &, const std::wstring &) { return false; }
std::wstring SecureStorage::loadApiKey(const std::wstring &) { return L""; }
bool SecureStorage::deleteApiKey(const std::wstring &) { return false; }
bool SecureStorage::hasApiKey(const std::wstring &) { return false; }
#endif
std::wstring SecureStorage::getStoragePath() { return L""; }
std::wstring SecureStorage::loadLegacyValue(const std::wstring &) { return L""; }

#ifndef NPPAI_PREVIEW_ENABLE_NETWORK
DWORD HttpClient::_timeoutMs = 0;
thread_local HttpClient::TransportObserver HttpClient::_threadTransportObserver;
void HttpClient::setThreadTransportObserver(TransportObserver) {}
HttpResponse HttpClient::post(const std::wstring &, const std::wstring &,
    const std::map<std::wstring, std::wstring> &, DWORD, bool) { return blocked(); }
HttpResponse HttpClient::get(const std::wstring &) { return blocked(); }
HttpResponse HttpClient::get(const std::wstring &,
    const std::map<std::wstring, std::wstring> &, DWORD, bool) { return blocked(); }
#endif

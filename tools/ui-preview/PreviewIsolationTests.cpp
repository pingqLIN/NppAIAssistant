#include "HttpClient.h"
#include "SecureStorage.h"
#include "SettingsStorage.h"
#include <iostream>

int main() {
#ifdef NPPAI_PREVIEW_ENABLE_NETWORK
  const bool blocked = true; // Live discovery is exercised by the separate probe.
  const bool sessionKey = SecureStorage::saveApiKey(L"synthetic", L"not-a-key") &&
      SecureStorage::loadApiKey(L"synthetic") == L"not-a-key" &&
      SecureStorage::hasApiKey(L"synthetic") &&
      SecureStorage::deleteApiKey(L"synthetic") &&
      SecureStorage::loadApiKey(L"synthetic").empty();
  const bool noStorage = SettingsStorage::getConfigFilePath().empty() &&
      SecureStorage::getStoragePath().empty() &&
      SecureStorage::loadLegacyValue(L"synthetic").empty() && sessionKey;
#else
  const bool blocked = !HttpClient::get(L"invalid:preview").success &&
      !HttpClient::get(L"invalid:preview", {}).success &&
      !HttpClient::post(L"invalid:preview", L"synthetic", {}).success;
  const bool noStorage = SettingsStorage::getConfigFilePath().empty() &&
      SecureStorage::getStoragePath().empty() &&
      !SecureStorage::saveApiKey(L"synthetic", L"not-a-key") &&
      SecureStorage::loadApiKey(L"synthetic").empty() &&
      SecureStorage::loadLegacyValue(L"synthetic").empty();
#endif
  SettingsStorage::saveString(L"preview-test", L"memory-only");
  const bool preferences = SettingsStorage::loadString(L"preview-test") == L"memory-only";
  if (!blocked || !noStorage || !preferences) return 1;
  std::cout << "PASS: selected preview profile storage contract\n";
}

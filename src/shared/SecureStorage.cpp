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

#include "SecureStorage.h"
#include <ShlObj.h>
#include <fstream>
#include <vector>
#include <wincrypt.h>


#pragma comment(lib, "crypt32.lib")

std::wstring SecureStorage::getStoragePath() {
  wchar_t appDataPath[MAX_PATH] = {};

  // Get AppData\Roaming path
  if (SUCCEEDED(
          SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
    std::wstring path = appDataPath;
    path += L"\\Notepad++\\AIAssistant";

    // Create directory if it doesn't exist
    CreateDirectory((std::wstring(appDataPath) + L"\\Notepad++").c_str(),
                    nullptr);
    CreateDirectory(path.c_str(), nullptr);

    return path;
  }

  return L"";
}

std::wstring SecureStorage::getKeyFilePath(const std::wstring &keyName) {
  std::wstring storagePath = getStoragePath();
  if (storagePath.empty())
    return L"";

  return storagePath + L"\\" + keyName + L".key";
}

std::vector<BYTE> SecureStorage::encrypt(const std::wstring &data) {
  std::vector<BYTE> encrypted;

  if (data.empty())
    return encrypted;

  // Convert wide string to bytes
  const BYTE *dataBytes = reinterpret_cast<const BYTE *>(data.c_str());
  DWORD dataSize = static_cast<DWORD>((data.length() + 1) * sizeof(wchar_t));

  // Set up input data blob
  DATA_BLOB inputBlob = {};
  inputBlob.pbData = const_cast<BYTE *>(dataBytes);
  inputBlob.cbData = dataSize;

  // Encrypt using DPAPI
  DATA_BLOB outputBlob = {};
  if (CryptProtectData(&inputBlob,
                       L"NppAIKey",               // Description
                       nullptr,                   // Optional entropy
                       nullptr,                   // Reserved
                       nullptr,                   // Optional prompt struct
                       CRYPTPROTECT_UI_FORBIDDEN, // No UI
                       &outputBlob)) {
    encrypted.assign(outputBlob.pbData, outputBlob.pbData + outputBlob.cbData);
    LocalFree(outputBlob.pbData);
  }

  return encrypted;
}

std::wstring SecureStorage::decrypt(const std::vector<BYTE> &data) {
  if (data.empty())
    return L"";

  // Set up input data blob
  DATA_BLOB inputBlob = {};
  inputBlob.pbData = const_cast<BYTE *>(data.data());
  inputBlob.cbData = static_cast<DWORD>(data.size());

  // Decrypt using DPAPI
  DATA_BLOB outputBlob = {};
  LPWSTR description = nullptr;

  if (CryptUnprotectData(&inputBlob, &description,
                         nullptr,                   // Optional entropy
                         nullptr,                   // Reserved
                         nullptr,                   // Optional prompt struct
                         CRYPTPROTECT_UI_FORBIDDEN, // No UI
                         &outputBlob)) {
    std::wstring result = reinterpret_cast<wchar_t *>(outputBlob.pbData);
    LocalFree(outputBlob.pbData);
    if (description)
      LocalFree(description);
    return result;
  }

  return L"";
}

bool SecureStorage::writeToFile(const std::wstring &filePath,
                                const std::vector<BYTE> &data) {
  std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
  if (!file.is_open())
    return false;

  file.write(reinterpret_cast<const char *>(data.data()), data.size());
  file.close();

  return true;
}

std::vector<BYTE> SecureStorage::readFromFile(const std::wstring &filePath) {
  std::vector<BYTE> data;

  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return data;

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  data.resize(static_cast<size_t>(size));
  file.read(reinterpret_cast<char *>(data.data()), size);
  file.close();

  return data;
}

bool SecureStorage::saveApiKey(const std::wstring &keyName,
                               const std::wstring &value) {
  if (keyName.empty())
    return false;

  std::wstring filePath = getKeyFilePath(keyName);
  if (filePath.empty())
    return false;

  // If value is empty, delete the key
  if (value.empty()) {
    return deleteApiKey(keyName);
  }

  // Encrypt and save
  std::vector<BYTE> encrypted = encrypt(value);
  if (encrypted.empty())
    return false;

  return writeToFile(filePath, encrypted);
}

std::wstring SecureStorage::loadApiKey(const std::wstring &keyName) {
  if (keyName.empty())
    return L"";

  std::wstring filePath = getKeyFilePath(keyName);
  if (filePath.empty())
    return L"";

  // Check if file exists
  DWORD attrs = GetFileAttributes(filePath.c_str());
  if (attrs == INVALID_FILE_ATTRIBUTES)
    return L"";

  // Read and decrypt
  std::vector<BYTE> encrypted = readFromFile(filePath);
  if (encrypted.empty())
    return L"";

  return decrypt(encrypted);
}

bool SecureStorage::deleteApiKey(const std::wstring &keyName) {
  if (keyName.empty())
    return false;

  std::wstring filePath = getKeyFilePath(keyName);
  if (filePath.empty())
    return false;

  return DeleteFile(filePath.c_str()) != 0;
}

bool SecureStorage::hasApiKey(const std::wstring &keyName) {
  if (keyName.empty())
    return false;

  std::wstring filePath = getKeyFilePath(keyName);
  if (filePath.empty())
    return false;

  DWORD attrs = GetFileAttributes(filePath.c_str());
  return (attrs != INVALID_FILE_ATTRIBUTES);
}

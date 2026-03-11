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

#pragma once

#include <string>
#include <vector>
#include <windows.h>

// Secure storage for sensitive data using Windows DPAPI
class SecureStorage {
public:
  // Save an API key securely (encrypted with DPAPI)
  // keyName: identifier for the key (e.g., "openai_key", "gemini_key")
  // value: the API key to store
  // Returns true on success
  static bool saveApiKey(const std::wstring &keyName,
                         const std::wstring &value);

  // Load an API key (decrypted from DPAPI storage)
  // keyName: identifier for the key
  // Returns empty string if not found or on error
  static std::wstring loadApiKey(const std::wstring &keyName);

  // Delete a stored API key
  // keyName: identifier for the key
  // Returns true on success
  static bool deleteApiKey(const std::wstring &keyName);

  // Check if an API key exists
  static bool hasApiKey(const std::wstring &keyName);

  // Get the storage directory path
  static std::wstring getStoragePath();

private:
  // Encrypt data using DPAPI
  static std::vector<BYTE> encrypt(const std::wstring &data);

  // Decrypt data using DPAPI
  static std::wstring decrypt(const std::vector<BYTE> &data);

  // Get full path for a key file
  static std::wstring getKeyFilePath(const std::wstring &keyName);

  // Write binary data to file
  static bool writeToFile(const std::wstring &filePath,
                          const std::vector<BYTE> &data);

  // Read binary data from file
  static std::vector<BYTE> readFromFile(const std::wstring &filePath);
};

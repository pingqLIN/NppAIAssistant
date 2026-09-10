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

// Plain settings storage for non-secret plugin preferences.
class SettingsStorage {
public:
  static std::wstring getConfigFilePath();

  static int loadSchemaVersion();
  static bool saveSchemaVersion(int version);

  // Aggregates write results for one caller-owned settings batch. This keeps a
  // migration from treating a partial INI write as a successful persistence.
  static void beginWriteBatch();
  static bool endWriteBatch();

  static std::wstring loadString(const std::wstring &keyName);
  static bool saveString(const std::wstring &keyName, const std::wstring &value);

private:
  static std::wstring escapeValue(const std::wstring &value);
  static std::wstring unescapeValue(const std::wstring &value);
};

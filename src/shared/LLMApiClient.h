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

#include <ctime>
#include <map>
#include <string>
#include <vector>
#include <windows.h>

struct HttpResponse;


struct LLMResponse {
  bool success = false;
  std::wstring content;
  std::wstring errorMessage;
  int tokensUsed = 0;
};

struct ModelListResponse {
  bool success = false;
  std::vector<std::wstring> models;
  std::wstring errorMessage;
};

struct CopilotDeviceCode {
  std::wstring deviceCode;
  std::wstring userCode;
  std::wstring verificationUri;
  int expiresIn = 0;
  int interval = 5;
};

struct CopilotTokens {
  std::wstring oauthToken;
  std::wstring copilotToken;
  time_t copilotTokenExpires = 0;
  bool isAuthenticated = false;
};

class LLMApiClient {
public:
  static LLMResponse callOpenAI(const std::wstring &apiKey,
                                const std::wstring &prompt,
                                const std::wstring &model,
                                DWORD timeoutMs);

  static LLMResponse callGemini(const std::wstring &apiKey,
                                const std::wstring &prompt,
                                const std::wstring &model,
                                DWORD timeoutMs);

  static LLMResponse
  callClaude(const std::wstring &apiKey, const std::wstring &prompt,
             const std::wstring &model, DWORD timeoutMs);

  static LLMResponse callOpenAICompatible(const std::wstring &baseUrl,
                                           const std::wstring &apiKey,
                                           const std::wstring &prompt,
                                           const std::wstring &model,
                                           DWORD timeoutMs);

  static ModelListResponse listOpenAIModels(const std::wstring &apiKey,
                                            DWORD timeoutMs);

  static ModelListResponse listGeminiModels(const std::wstring &apiKey,
                                            DWORD timeoutMs);

  static ModelListResponse listClaudeModels(const std::wstring &apiKey,
                                            DWORD timeoutMs);

  static ModelListResponse listOpenAICompatibleModels(
      const std::wstring &baseUrl, const std::wstring &apiKey, DWORD timeoutMs);

  static bool normalizeLoopbackCompatibleBaseUrl(
      const std::wstring &input, std::wstring &canonicalBaseUrl);

  static CopilotDeviceCode initiateCopilotDeviceFlow();
  
  // Returns: 1=success, 0=pending (keep polling), -1=error (stop polling)
  static int pollCopilotAccessToken(const std::wstring &deviceCode,
                                    CopilotTokens &tokens);
  
  static bool refreshCopilotToken(CopilotTokens &tokens);
  
  static LLMResponse callCopilot(CopilotTokens &tokens,
                                 const std::wstring &prompt,
                                 const std::wstring &model = L"gpt-4o");

  static std::wstring getLastCopilotAuthDebug();

  static std::wstring escapeJsonString(const std::wstring &input);

 private:
  static void setLastCopilotAuthDebug(const std::wstring &value);

  static std::wstring extractJsonValue(const std::wstring &json,
                                       const std::wstring &key);

  static std::wstring extractJsonPath(const std::wstring &json,
                                      const std::wstring &path);

  static std::wstring sanitizeAuthBody(const std::wstring &body);
  static std::wstring buildAuthDebugMessage(const HttpResponse &httpResponse);
  
  static constexpr const wchar_t* COPILOT_CLIENT_ID = L"Iv1.b507a08c87ecfe98";
  static std::wstring _lastCopilotAuthDebug;
};

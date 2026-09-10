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

#include "LLMApiClient.h"
#include "HttpClient.h"
#include <algorithm>
#include <cwctype>
#include <regex>
#include <sstream>

namespace {
const wchar_t *kGitHubUserAgent = L"Notepad++ AI Assistant/1.0";
constexpr DWORD kLoopbackDiscoveryTimeoutMs = 1500;
constexpr DWORD kLoopbackGenerationTimeoutMs = 900 * 1000;

wchar_t hexToValue(wchar_t ch) {
  if (ch >= L'0' && ch <= L'9')
    return static_cast<wchar_t>(ch - L'0');
  if (ch >= L'a' && ch <= L'f')
    return static_cast<wchar_t>(ch - L'a' + 10);
  if (ch >= L'A' && ch <= L'F')
    return static_cast<wchar_t>(ch - L'A' + 10);
  return 0;
}

std::wstring urlDecode(const std::wstring &value) {
  std::wstring decoded;
  decoded.reserve(value.size());

  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == L'%' && i + 2 < value.size()) {
      wchar_t high = value[i + 1];
      wchar_t low = value[i + 2];
      if (((high >= L'0' && high <= L'9') || (high >= L'a' && high <= L'f') ||
           (high >= L'A' && high <= L'F')) &&
          ((low >= L'0' && low <= L'9') || (low >= L'a' && low <= L'f') ||
           (low >= L'A' && low <= L'F'))) {
        wchar_t decodedChar = static_cast<wchar_t>((hexToValue(high) << 4) | hexToValue(low));
        decoded.push_back(decodedChar);
        i += 2;
        continue;
      }
    }

    if (value[i] == L'+') {
      decoded.push_back(L' ');
    } else {
      decoded.push_back(value[i]);
    }
  }

  return decoded;
}

std::wstring extractFormValue(const std::wstring &body, const std::wstring &key) {
  size_t pos = 0;
  while (pos < body.size()) {
    size_t ampPos = body.find(L'&', pos);
    if (ampPos == std::wstring::npos)
      ampPos = body.size();

    size_t eqPos = body.find(L'=', pos);
    if (eqPos != std::wstring::npos && eqPos < ampPos) {
      std::wstring pairKey = body.substr(pos, eqPos - pos);
      if (pairKey == key) {
        std::wstring rawValue = body.substr(eqPos + 1, ampPos - eqPos - 1);
        return urlDecode(rawValue);
      }
    }

    if (ampPos == body.size())
      break;
    pos = ampPos + 1;
  }

  return L"";
}

std::wstring redactValue(const std::wstring &body, const std::wstring &value) {
  if (value.empty())
    return body;

  std::wstring result = body;
  size_t pos = 0;
  while ((pos = result.find(value, pos)) != std::wstring::npos) {
    result.replace(pos, value.size(), L"<redacted>");
    pos += 10;
  }

  return result;
}

std::wstring sanitizeUntrustedProviderError(const std::wstring &message,
                                            const std::wstring &knownApiKey) {
  // Redact before bounding so a credential that crosses the display limit
  // cannot survive as a partial secret after truncation.
  std::wstring sanitized = redactValue(message, knownApiKey);
  sanitized = std::regex_replace(
      sanitized,
      std::wregex(
          LR"((?:sk[-_]|hf_|gh[pousr]_|github_pat_)[A-Za-z0-9_-]{8,}|Bearer\s+[A-Za-z0-9._-]{8,})"),
      L"<redacted>");
  if (sanitized.size() > 1024) sanitized.resize(1024);
  for (wchar_t &ch : sanitized) {
    if (ch < 0x20 && ch != L'\n' && ch != L'\r' && ch != L'\t') ch = L' ';
  }
  return sanitized;
}

bool startsWith(const std::wstring &value, const std::wstring &prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

std::wstring compatibleEndpoint(const std::wstring &baseUrl,
                                const wchar_t *endpoint) {
  std::wstring base = baseUrl;
  while (!base.empty() && base.back() == L'/') base.pop_back();
  return base + endpoint;
}

bool isUsableCompatibleModel(const std::wstring &model) {
  std::wstring lower = model;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
  return lower.find(L"embedding") == std::wstring::npos &&
         lower.find(L"mmproj") == std::wstring::npos;
}

int jsonHexDigit(wchar_t ch) {
  if (ch >= L'0' && ch <= L'9') return ch - L'0';
  if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
  if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
  return -1;
}

bool decodeJsonString(const std::wstring &json, size_t quote,
                      std::wstring &value, size_t &next, size_t maxChars) {
  value.clear();
  if (quote >= json.size() || json[quote] != L'"') return false;
  for (size_t i = quote + 1; i < json.size(); ++i) {
    wchar_t ch = json[i];
    if (ch == L'"') { next = i + 1; return true; }
    if (ch < 0x20 || value.size() >= maxChars) return false;
    if (ch != L'\\') { value.push_back(ch); continue; }
    if (++i >= json.size()) return false;
    switch (json[i]) {
    case L'"': value.push_back(L'"'); break;
    case L'\\': value.push_back(L'\\'); break;
    case L'/': value.push_back(L'/'); break;
    case L'b': value.push_back(L'\b'); break;
    case L'f': value.push_back(L'\f'); break;
    case L'n': value.push_back(L'\n'); break;
    case L'r': value.push_back(L'\r'); break;
    case L't': value.push_back(L'\t'); break;
    case L'u': {
      if (i + 4 >= json.size()) return false;
      unsigned int codeUnit = 0;
      for (size_t digit = 1; digit <= 4; ++digit) {
        const int valueDigit = jsonHexDigit(json[i + digit]);
        if (valueDigit < 0) return false;
        codeUnit = (codeUnit << 4) | static_cast<unsigned int>(valueDigit);
      }
      value.push_back(static_cast<wchar_t>(codeUnit));
      i += 4;
      break;
    }
    default: return false;
    }
  }
  return false;
}

std::vector<std::wstring> extractBoundedJsonStringFields(
    const std::wstring &json, const wchar_t *key, size_t limit) {
  std::vector<std::wstring> values;
  const std::wstring marker = L"\"" + std::wstring(key) + L"\"";
  size_t pos = 0;
  while (values.size() < limit && (pos = json.find(marker, pos)) != std::wstring::npos) {
    size_t colon = json.find(L':', pos + marker.size());
    if (colon == std::wstring::npos) break;
    size_t quote = json.find(L'"', colon + 1);
    if (quote == std::wstring::npos) break;
    std::wstring value;
    size_t next = quote;
    if (decodeJsonString(json, quote, value, next, 512)) values.push_back(value);
    pos = next;
  }
  return values;
}

void addUniqueModel(std::vector<std::wstring> &models, const std::wstring &model) {
  if (model.empty()) {
    return;
  }

  if (std::find(models.begin(), models.end(), model) == models.end()) {
    models.push_back(model);
  }
}

bool isLikelyOpenAIChatModel(const std::wstring &model) {
  return startsWith(model, L"gpt-") || startsWith(model, L"chatgpt-") ||
         startsWith(model, L"o1") || startsWith(model, L"o3") ||
         startsWith(model, L"o4");
}

bool appendStructuredOutputFormat(const StructuredOutputConfig &config,
                                  CompatibleApiMode mode,
                                  std::wstring &requestBody,
                                  std::wstring &error) {
  const StructuredOutputFormatResult format = buildStructuredOutputFormat(
      config, mode == CompatibleApiMode::Responses
                  ? StructuredOutputTransport::Responses
                  : StructuredOutputTransport::ChatCompletions);
  if (!format.success) {
    error = format.errorMessage;
    return false;
  }
  requestBody += format.jsonMember;
  return true;
}

void applyEnvelopeResult(LLMResponse &response,
                         const ProviderEnvelopeResult &envelope,
                         const StructuredOutputConfig &structuredOutput,
                         const std::wstring &knownApiKey) {
  response.failure = envelope.failure;
  if (!envelope.success) {
    response.rawContent =
        sanitizeUntrustedProviderError(envelope.content, knownApiKey);
    response.errorMessage =
        sanitizeUntrustedProviderError(envelope.errorMessage, knownApiKey);
    return;
  }
  const std::wstring safeContent = redactValue(envelope.content, knownApiKey);
  response.rawContent = safeContent;
  if (structuredOutput.enabled) {
    if (!envelope.completionStatusKnown) {
      response.failure = ResponseFailure::ProviderResponseError;
      response.errorMessage =
          L"Structured response did not include a completion status.";
      return;
    }
    const StructuredOutputValidationResult validation =
        validateStructuredOutputContent(safeContent, structuredOutput);
    if (!validation.success) {
      response.failure = validation.failure;
      response.errorMessage =
          sanitizeUntrustedProviderError(validation.errorMessage, knownApiKey);
      return;
    }
  }
  response.success = true;
  response.failure = ResponseFailure::None;
  response.content = safeContent;
}

}


std::wstring LLMApiClient::_lastCopilotAuthDebug;

std::wstring LLMApiClient::getLastCopilotAuthDebug() {
  return _lastCopilotAuthDebug;
}

void LLMApiClient::setLastCopilotAuthDebug(const std::wstring &value) {
  _lastCopilotAuthDebug = value;
}

std::wstring LLMApiClient::sanitizeAuthBody(const std::wstring &body) {
  static const wchar_t *const sensitiveMarkers[] = {
      L"\"access_token\"", L"\"device_code\"", L"\"user_code\"",
      L"\"token\"",        L"access_token=",       L"device_code=",
      L"user_code=",        L"token="};
  for (const wchar_t *marker : sensitiveMarkers) {
    if (body.find(marker) != std::wstring::npos) {
      return L"<redacted authentication response>";
    }
  }

  std::wstring sanitized = body;

  std::wstring accessToken = extractJsonValue(body, L"access_token");
  if (accessToken.empty())
    accessToken = extractFormValue(body, L"access_token");
  sanitized = redactValue(sanitized, accessToken);

  std::wstring deviceCode = extractJsonValue(body, L"device_code");
  if (deviceCode.empty())
    deviceCode = extractFormValue(body, L"device_code");
  sanitized = redactValue(sanitized, deviceCode);

  std::wstring userCode = extractJsonValue(body, L"user_code");
  if (userCode.empty())
    userCode = extractFormValue(body, L"user_code");
  sanitized = redactValue(sanitized, userCode);

  std::wstring copilotToken = extractJsonValue(body, L"token");
  sanitized = redactValue(sanitized, copilotToken);

  return sanitizeUntrustedProviderError(sanitized, L"");
}

std::wstring LLMApiClient::buildAuthDebugMessage(const HttpResponse &httpResponse) {
  std::wstring message = L"status=" + std::to_wstring(httpResponse.statusCode);
  if (!httpResponse.errorMessage.empty()) {
    message += L"\nerror=" + httpResponse.errorMessage;
  }
  if (!httpResponse.body.empty()) {
    message += L"\nbody:\n" + sanitizeAuthBody(httpResponse.body);
  }
  return sanitizeUntrustedProviderError(message, L"");
}

std::wstring LLMApiClient::escapeJsonString(const std::wstring &input) {
  std::wstring result;
  result.reserve(input.size() * 2);

  for (wchar_t ch : input) {
    switch (ch) {
    case L'\"':
      result += L"\\\"";
      break;
    case L'\\':
      result += L"\\\\";
      break;
    case L'\b':
      result += L"\\b";
      break;
    case L'\f':
      result += L"\\f";
      break;
    case L'\n':
      result += L"\\n";
      break;
    case L'\r':
      result += L"\\r";
      break;
    case L'\t':
      result += L"\\t";
      break;
    default:
      if (ch < 0x20) {
        wchar_t buf[8];
        swprintf_s(buf, L"\\u%04x", ch);
        result += buf;
      } else {
        result += ch;
      }
      break;
    }
  }
  return result;
}

std::wstring LLMApiClient::extractJsonValue(const std::wstring &json,
                                            const std::wstring &key) {
  // Simple JSON value extraction - looks for "key": "value" or "key": value
  std::wstring searchKey = L"\"" + key + L"\"";
  size_t keyPos = json.find(searchKey);

  if (keyPos == std::wstring::npos)
    return L"";

  size_t colonPos = json.find(L':', keyPos + searchKey.length());
  if (colonPos == std::wstring::npos)
    return L"";

  // Skip whitespace after colon
  size_t valueStart = colonPos + 1;
  while (valueStart < json.length() &&
         (json[valueStart] == L' ' || json[valueStart] == L'\t' ||
          json[valueStart] == L'\n' || json[valueStart] == L'\r'))
    valueStart++;

  if (valueStart >= json.length())
    return L"";

  // Check if value is a string (starts with quote)
  if (json[valueStart] == L'"') {
    valueStart++;
    std::wstring value;
    bool escaped = false;
    bool closed = false;

    for (size_t i = valueStart; i < json.length(); ++i) {
      if (escaped) {
        switch (json[i]) {
        case L'n':
          value += L'\n';
          break;
        case L'r':
          value += L'\r';
          break;
        case L't':
          value += L'\t';
          break;
        case L'"':
          value += L'"';
          break;
        case L'\\':
          value += L'\\';
          break;
        default:
          value += json[i];
          break;
        }
        escaped = false;
      } else if (json[i] == L'\\') {
        escaped = true;
      } else if (json[i] == L'"') {
        closed = true;
        break;
      } else {
        value += json[i];
      }
    }
    if (!closed) return L"";
    return value;
  }

  // For non-string values (numbers, booleans, etc.)
  size_t valueEnd = valueStart;
  while (valueEnd < json.length() && json[valueEnd] != L',' &&
         json[valueEnd] != L'}' && json[valueEnd] != L']')
    valueEnd++;

  return json.substr(valueStart, valueEnd - valueStart);
}

std::wstring LLMApiClient::extractJsonPath(const std::wstring &json,
                                           const std::wstring &path) {
  // Simple path extraction for nested JSON
  // Example: "choices.0.message.content"
  std::wstring current = json;
  std::wstring segment;
  std::wstringstream ss(path);

  while (std::getline(ss, segment, L'.')) {
    // Check if segment is an array index
    bool isIndex = !segment.empty() &&
                   std::all_of(segment.begin(), segment.end(), ::iswdigit);

    if (isIndex) {
      int index = std::stoi(segment);
      // Find array start
      size_t arrStart = current.find(L'[');
      if (arrStart == std::wstring::npos)
        return L"";

      // Skip to the nth element
      int count = 0;
      size_t pos = arrStart + 1;
      int depth = 0;
      size_t elemStart = pos;

      while (pos < current.length()) {
        if (current[pos] == L'{' || current[pos] == L'[')
          depth++;
        else if (current[pos] == L'}' || current[pos] == L']')
          depth--;
        else if ((current[pos] == L',' || current[pos] == L']') && depth == 0) {
          if (count == index) {
            current = current.substr(elemStart, pos - elemStart);
            break;
          }
          count++;
          elemStart = pos + 1;
          // Skip whitespace
          while (elemStart < current.length() &&
                 (current[elemStart] == L' ' || current[elemStart] == L'\n' ||
                  current[elemStart] == L'\r' || current[elemStart] == L'\t'))
            elemStart++;
        }
        pos++;
      }
    } else {
      current = extractJsonValue(current, segment);
    }

    if (current.empty())
      return L"";
  }

  return current;
}

ModelListResponse LLMApiClient::listOpenAIModels(const std::wstring &apiKey) {
  ModelListResponse response;

  if (apiKey.empty()) {
    response.errorMessage = L"OpenAI API key is not configured";
    return response;
  }

  std::map<std::wstring, std::wstring> headers;
  headers[L"Authorization"] = L"Bearer " + apiKey;

  HttpResponse httpResponse = HttpClient::get(L"https://api.openai.com/v1/models", headers);
  if (!httpResponse.success) {
    response.errorMessage = L"HTTP request failed: " + httpResponse.errorMessage;
    if (!httpResponse.body.empty()) {
      std::wstring errorMsg = extractJsonValue(httpResponse.body, L"message");
      if (!errorMsg.empty()) {
        response.errorMessage +=
            L"\n" + sanitizeUntrustedProviderError(errorMsg, apiKey);
      }
    }
    return response;
  }

  std::wregex idPattern(L"\"id\"\\s*:\\s*\"([^\"]+)\"");
  for (std::wsregex_iterator it(httpResponse.body.begin(), httpResponse.body.end(),
                                idPattern),
       end;
       it != end; ++it) {
    const std::wstring model = (*it)[1].str();
    if (isLikelyOpenAIChatModel(model)) {
      addUniqueModel(response.models, model);
    }
  }

  if (response.models.empty()) {
    response.errorMessage = L"OpenAI returned no chat models";
    return response;
  }

  response.success = true;
  return response;
}

ModelListResponse LLMApiClient::listGeminiModels(const std::wstring &apiKey) {
  ModelListResponse response;

  if (apiKey.empty()) {
    response.errorMessage = L"Gemini API key is not configured";
    return response;
  }

  std::map<std::wstring, std::wstring> headers;
  headers[L"x-goog-api-key"] = apiKey;
  HttpResponse httpResponse =
      HttpClient::get(L"https://generativelanguage.googleapis.com/v1beta/models",
                      headers);
  if (!httpResponse.success) {
    response.errorMessage = L"HTTP request failed: " + httpResponse.errorMessage;
    if (!httpResponse.body.empty()) {
      std::wstring errorMsg = extractJsonValue(httpResponse.body, L"message");
      if (!errorMsg.empty()) {
        response.errorMessage +=
            L"\n" + sanitizeUntrustedProviderError(errorMsg, apiKey);
      }
    }
    return response;
  }

  std::wregex modelPattern(
      L"\"name\"\\s*:\\s*\"models/([^\"]+)\"[\\s\\S]*?\"supportedGenerationMethods\"\\s*:\\s*\\[([\\s\\S]*?)\\]");
  for (std::wsregex_iterator it(httpResponse.body.begin(), httpResponse.body.end(),
                                modelPattern),
       end;
       it != end; ++it) {
    std::wstring model = (*it)[1].str();
    std::wstring methods = (*it)[2].str();
    if (methods.find(L"generateContent") != std::wstring::npos &&
        startsWith(model, L"gemini")) {
      addUniqueModel(response.models, model);
    }
  }

  if (response.models.empty()) {
    std::wregex fallbackPattern(L"\"name\"\\s*:\\s*\"models/(gemini[^\"]+)\"");
    for (std::wsregex_iterator it(httpResponse.body.begin(), httpResponse.body.end(),
                                  fallbackPattern),
         end;
         it != end; ++it) {
      addUniqueModel(response.models, (*it)[1].str());
    }
  }

  if (response.models.empty()) {
    response.errorMessage = L"Gemini returned no compatible models";
    return response;
  }

  response.success = true;
  return response;
}

ModelListResponse LLMApiClient::listClaudeModels(const std::wstring &apiKey) {
  ModelListResponse response;

  if (apiKey.empty()) {
    response.errorMessage = L"Claude API key is not configured";
    return response;
  }

  std::map<std::wstring, std::wstring> headers;
  headers[L"x-api-key"] = apiKey;
  headers[L"anthropic-version"] = L"2023-06-01";

  HttpResponse httpResponse = HttpClient::get(L"https://api.anthropic.com/v1/models", headers);
  if (!httpResponse.success) {
    response.errorMessage = L"HTTP request failed: " + httpResponse.errorMessage;
    if (!httpResponse.body.empty()) {
      std::wstring errorMsg = extractJsonValue(httpResponse.body, L"message");
      if (!errorMsg.empty()) {
        response.errorMessage +=
            L"\n" + sanitizeUntrustedProviderError(errorMsg, apiKey);
      }
    }
    return response;
  }

  std::wregex idPattern(L"\"id\"\\s*:\\s*\"([^\"]+)\"");
  for (std::wsregex_iterator it(httpResponse.body.begin(), httpResponse.body.end(),
                                idPattern),
       end;
       it != end; ++it) {
    addUniqueModel(response.models, (*it)[1].str());
  }

  if (response.models.empty()) {
    response.errorMessage = L"Claude returned no models";
    return response;
  }

  response.success = true;
  return response;
}

bool isKnownClaudePromptCacheModel(const std::wstring &model) {
  return startsWith(model, L"claude-sonnet-4-") ||
         startsWith(model, L"claude-opus-4-") ||
         startsWith(model, L"claude-haiku-4-");
}

ModelListResponse LLMApiClient::listOpenAICompatibleModels(
    const std::wstring &baseUrl, const std::wstring &apiKey, bool loopback) {
  ModelListResponse response;
  if (baseUrl.empty()) {
    response.errorMessage = L"Compatible service base URL is not configured";
    return response;
  }
  std::map<std::wstring, std::wstring> headers;
  if (!apiKey.empty()) headers[L"Authorization"] = L"Bearer " + apiKey;
  HttpResponse httpResponse = HttpClient::get(
      compatibleEndpoint(baseUrl, L"/models"), headers,
      loopback ? kLoopbackDiscoveryTimeoutMs : 0, loopback);
  if (!httpResponse.success) {
    response.errorMessage = httpResponse.statusCode == 401
                                ? L"Service is reachable but requires authentication"
                                : L"HTTP request failed: " + httpResponse.errorMessage;
    return response;
  }
  for (const std::wstring &model :
       extractBoundedJsonStringFields(httpResponse.body, L"id", 128)) {
    const bool safeModelId = !model.empty() && model.size() <= 256 &&
        std::none_of(model.begin(), model.end(), [](wchar_t ch) {
          return ch < 0x20 || (ch >= 0x7F && ch <= 0x9F);
        });
    if (safeModelId && isUsableCompatibleModel(model)) addUniqueModel(response.models, model);
  }
  if (response.models.empty()) {
    response.errorMessage = L"Compatible service returned no generation models";
    return response;
  }
  response.success = true;
  return response;
}

LLMResponse LLMApiClient::callOpenAICompatible(
    const std::wstring &baseUrl, const std::wstring &apiKey,
    const std::wstring &prompt, const std::wstring &model,
    CompatibleApiMode mode, bool loopback,
    const StructuredOutputConfig &structuredOutput) {
  LLMResponse response;
  if (baseUrl.empty() || model.empty()) {
    response.errorMessage = L"Compatible service URL or model is missing";
    response.failure = ResponseFailure::ProviderResponseError;
    return response;
  }
  const std::wstring escapedPrompt = escapeJsonString(prompt);
  std::wstring requestBody;
  const wchar_t *endpoint = L"/chat/completions";
  if (mode == CompatibleApiMode::Responses) {
    endpoint = L"/responses";
    requestBody = L"{\"model\":\"" + escapeJsonString(model) +
                   L"\",\"input\":\"" + escapedPrompt +
                   L"\",\"store\":false,\"stream\":false";
  } else {
    requestBody = L"{\"model\":\"" + escapeJsonString(model) +
                   L"\",\"messages\":[{\"role\":\"user\",\"content\":\"" +
                   escapedPrompt + L"\"}],\"stream\":false";
  }
  if (!appendStructuredOutputFormat(structuredOutput, mode, requestBody,
                                    response.errorMessage)) {
    response.failure = ResponseFailure::SchemaValidationError;
    return response;
  }
  requestBody += L"}";
  std::map<std::wstring, std::wstring> headers;
  headers[L"Content-Type"] = L"application/json";
  if (!apiKey.empty()) headers[L"Authorization"] = L"Bearer " + apiKey;
  HttpResponse httpResponse = HttpClient::post(
      compatibleEndpoint(baseUrl, endpoint), requestBody, headers,
      loopback ? kLoopbackGenerationTimeoutMs : 0, loopback);
  if (!httpResponse.success) {
    if (httpResponse.statusCode == 401) {
      response.errorMessage = L"Compatible service rejected authentication (HTTP 401).";
    } else {
      response.errorMessage = L"Compatible service request failed.";
      if (httpResponse.statusCode != 0) {
        response.errorMessage += L" HTTP " + std::to_wstring(httpResponse.statusCode) + L".";
      }
      if (httpResponse.statusCode == 400 || httpResponse.statusCode == 422) {
        response.errorMessage += L" Check the selected model, API mode and output schema; consult the service log for details.";
      } else if (httpResponse.statusCode == 404) {
        response.errorMessage += L" Check the Base URL and whether this service supports the selected API mode.";
      } else if (httpResponse.statusCode >= 500) {
        response.errorMessage += L" The service reported a server error; check its model loading and inference log.";
      }
      if (!httpResponse.errorMessage.empty()) {
        response.errorMessage += L" " + sanitizeUntrustedProviderError(
                                            httpResponse.errorMessage, apiKey);
      }
    }
    response.failure = ResponseFailure::HttpError;
    return response;
  }
  const ProviderEnvelopeResult envelope =
      mode == CompatibleApiMode::Responses
          ? extractResponsesEnvelope(httpResponse.body)
          : extractChatCompletionEnvelope(httpResponse.body);
  applyEnvelopeResult(response, envelope, structuredOutput, apiKey);
  return response;
}

LLMResponse LLMApiClient::callOpenAI(const std::wstring &apiKey,
                                     const std::wstring &prompt,
                                     const std::wstring &model,
                                     const StructuredOutputConfig &structuredOutput) {
  LLMResponse response;

  if (apiKey.empty()) {
    response.errorMessage = L"OpenAI API key is not configured";
    response.failure = ResponseFailure::ProviderResponseError;
    return response;
  }

  // Build request body
  std::wstring escapedPrompt = escapeJsonString(prompt);
  std::wstring requestBody =
      L"{\"model\":\"" + escapeJsonString(model) +
      L"\","
      L"\"messages\":[{\"role\":\"user\",\"content\":\"" +
      escapedPrompt +
      L"\"}],"
      L"\"stream\":false,"
      L"\"max_tokens\":2048";
  if (!appendStructuredOutputFormat(structuredOutput,
                                    CompatibleApiMode::ChatCompletions,
                                    requestBody, response.errorMessage)) {
    response.failure = ResponseFailure::SchemaValidationError;
    return response;
  }
  requestBody += L"}";

  // Set headers
  std::map<std::wstring, std::wstring> headers;
  headers[L"Content-Type"] = L"application/json";
  headers[L"Authorization"] = L"Bearer " + apiKey;

  // Make request
  HttpResponse httpResponse = HttpClient::post(
      L"https://api.openai.com/v1/chat/completions", requestBody, headers);

  if (!httpResponse.success) {
    response.errorMessage =
        L"HTTP request failed: " + httpResponse.errorMessage;
    if (!httpResponse.body.empty()) {
      // Try to extract error message from response
      std::wstring errorMsg = extractJsonValue(httpResponse.body, L"message");
      if (!errorMsg.empty())
        response.errorMessage += L"\n" +
                                 sanitizeUntrustedProviderError(errorMsg, apiKey);
    }
    response.failure = ResponseFailure::HttpError;
    return response;
  }

  applyEnvelopeResult(response, extractChatCompletionEnvelope(httpResponse.body),
                      structuredOutput, apiKey);

  return response;
}

LLMResponse LLMApiClient::callGemini(const std::wstring &apiKey,
                                     const std::wstring &prompt,
                                     const std::wstring &model) {
  LLMResponse response;

  if (apiKey.empty()) {
    response.errorMessage = L"Gemini API key is not configured";
    return response;
  }

  std::wstring url =
      L"https://generativelanguage.googleapis.com/v1beta/models/" + model +
      L":generateContent";

  // Build request body
  std::wstring escapedPrompt = escapeJsonString(prompt);
  std::wstring requestBody =
      L"{\"contents\":[{\"parts\":[{\"text\":\"" + escapedPrompt + L"\"}]}]}";

  // Set headers
  std::map<std::wstring, std::wstring> headers;
  headers[L"Content-Type"] = L"application/json";
  headers[L"x-goog-api-key"] = apiKey;

  // Make request
  HttpResponse httpResponse = HttpClient::post(url, requestBody, headers);

  if (!httpResponse.success) {
    response.errorMessage =
        L"HTTP request failed: " + httpResponse.errorMessage;
    if (!httpResponse.body.empty()) {
      std::wstring errorMsg = extractJsonValue(httpResponse.body, L"message");
      if (!errorMsg.empty())
        response.errorMessage +=
            L"\n" + sanitizeUntrustedProviderError(errorMsg, apiKey);
    }
    response.failure = ResponseFailure::HttpError;
    return response;
  }

  applyEnvelopeResult(response, extractGeminiEnvelope(httpResponse.body), {},
                      apiKey);

  return response;
}

LLMResponse LLMApiClient::callClaude(const std::wstring &apiKey,
                                     const std::wstring &prompt,
                                     const std::wstring &model,
                                     const std::wstring &cacheStablePrefix) {
  LLMResponse response;

  if (apiKey.empty()) {
    response.errorMessage = L"Claude API key is not configured";
    return response;
  }

  // Build request body
  std::wstring escapedPrompt = escapeJsonString(prompt);
  std::wstring content;
  if (isKnownClaudePromptCacheModel(model) && !cacheStablePrefix.empty() &&
      prompt.rfind(cacheStablePrefix, 0) == 0) {
    const std::wstring dynamicSuffix = prompt.substr(cacheStablePrefix.size());
    content = L"[{\"type\":\"text\",\"text\":\"" +
              escapeJsonString(cacheStablePrefix) +
              L"\",\"cache_control\":{\"type\":\"ephemeral\"}},"
              L"{\"type\":\"text\",\"text\":\"" +
              escapeJsonString(dynamicSuffix) + L"\"}]";
  } else {
    content = L"\"" + escapedPrompt + L"\"";
  }
  std::wstring requestBody =
      L"{\"model\":\"" + model +
      L"\","
      L"\"max_tokens\":2048,"
      L"\"messages\":[{\"role\":\"user\",\"content\":" +
      content + L"}]}";

  // Set headers
  std::map<std::wstring, std::wstring> headers;
  headers[L"Content-Type"] = L"application/json";
  headers[L"x-api-key"] = apiKey;
  headers[L"anthropic-version"] = L"2023-06-01";

  // Make request
  HttpResponse httpResponse = HttpClient::post(
      L"https://api.anthropic.com/v1/messages", requestBody, headers);

  if (!httpResponse.success) {
    response.errorMessage =
        L"HTTP request failed: " + httpResponse.errorMessage;
    if (!httpResponse.body.empty()) {
      std::wstring errorMsg = extractJsonValue(httpResponse.body, L"message");
      if (!errorMsg.empty())
        response.errorMessage +=
            L"\n" + sanitizeUntrustedProviderError(errorMsg, apiKey);
    }
    response.failure = ResponseFailure::HttpError;
    return response;
  }

  applyEnvelopeResult(response, extractClaudeEnvelope(httpResponse.body), {},
                      apiKey);

  return response;
}

CopilotDeviceCode LLMApiClient::initiateCopilotDeviceFlow() {
  CopilotDeviceCode result;
  
  std::wstring requestBody = L"client_id=";
  requestBody += COPILOT_CLIENT_ID;
  requestBody += L"&scope=user:email";
  
  std::map<std::wstring, std::wstring> headers;
  headers[L"Accept"] = L"application/json";
  headers[L"Content-Type"] = L"application/x-www-form-urlencoded";
  headers[L"User-Agent"] = kGitHubUserAgent;
  
  HttpResponse httpResponse = HttpClient::post(
      L"https://github.com/login/device/code", requestBody, headers);
  
  if (httpResponse.success) {
    result.deviceCode = extractJsonValue(httpResponse.body, L"device_code");
    result.userCode = extractJsonValue(httpResponse.body, L"user_code");
    result.verificationUri = extractJsonValue(httpResponse.body, L"verification_uri");

    if (result.deviceCode.empty())
      result.deviceCode = extractFormValue(httpResponse.body, L"device_code");
    if (result.userCode.empty())
      result.userCode = extractFormValue(httpResponse.body, L"user_code");
    if (result.verificationUri.empty())
      result.verificationUri = extractFormValue(httpResponse.body, L"verification_uri");
    
    std::wstring expiresStr = extractJsonValue(httpResponse.body, L"expires_in");
    if (expiresStr.empty())
      expiresStr = extractFormValue(httpResponse.body, L"expires_in");
    if (!expiresStr.empty()) {
      result.expiresIn = std::stoi(expiresStr);
    }
    
    std::wstring intervalStr = extractJsonValue(httpResponse.body, L"interval");
    if (intervalStr.empty())
      intervalStr = extractFormValue(httpResponse.body, L"interval");
    if (!intervalStr.empty()) {
      result.interval = std::stoi(intervalStr);
    }
  }

  if (result.deviceCode.empty() || result.userCode.empty()) {
    setLastCopilotAuthDebug(buildAuthDebugMessage(httpResponse));
  } else {
    setLastCopilotAuthDebug(L"");
  }
  
  return result;
}

int LLMApiClient::pollCopilotAccessToken(const std::wstring &deviceCode,
                                         CopilotTokens &tokens) {
  std::wstring requestBody = L"client_id=";
  requestBody += COPILOT_CLIENT_ID;
  requestBody += L"&device_code=" + deviceCode;
  requestBody += L"&grant_type=urn:ietf:params:oauth:grant-type:device_code";
  
  std::map<std::wstring, std::wstring> headers;
  headers[L"Accept"] = L"application/json";
  headers[L"Content-Type"] = L"application/x-www-form-urlencoded";
  headers[L"User-Agent"] = kGitHubUserAgent;
  
  HttpResponse httpResponse = HttpClient::post(
      L"https://github.com/login/oauth/access_token", requestBody, headers);
  
  std::wstring accessToken = extractJsonValue(httpResponse.body, L"access_token");
  if (accessToken.empty())
    accessToken = extractFormValue(httpResponse.body, L"access_token");
  if (!accessToken.empty()) {
    tokens.oauthToken = accessToken;
    if (refreshCopilotToken(tokens)) {
      setLastCopilotAuthDebug(L"");
      return 1;
    }
    setLastCopilotAuthDebug(buildAuthDebugMessage(httpResponse));
    return -1;
  }

  std::wstring error = extractJsonValue(httpResponse.body, L"error");
  if (error.empty())
    error = extractFormValue(httpResponse.body, L"error");
  if (error == L"authorization_pending" || error == L"slow_down") {
    setLastCopilotAuthDebug(buildAuthDebugMessage(httpResponse));
    return 0;
  }

  if (!error.empty()) {
    setLastCopilotAuthDebug(buildAuthDebugMessage(httpResponse));
    return -1;
  }

  if (httpResponse.success) {
    setLastCopilotAuthDebug(buildAuthDebugMessage(httpResponse));
    return -1;
  }

  setLastCopilotAuthDebug(buildAuthDebugMessage(httpResponse));
  return -1;
}

bool LLMApiClient::refreshCopilotToken(CopilotTokens &tokens) {
  if (tokens.oauthToken.empty()) {
    return false;
  }
  
  std::map<std::wstring, std::wstring> headers;
  headers[L"Accept"] = L"application/json";
  headers[L"Authorization"] = L"Bearer " + tokens.oauthToken;
  headers[L"User-Agent"] = kGitHubUserAgent;
  
  HttpResponse httpResponse = HttpClient::get(
      L"https://api.github.com/copilot_internal/v2/token", headers);
  
  if (httpResponse.success) {
    std::wstring token = extractJsonValue(httpResponse.body, L"token");
    if (!token.empty()) {
      tokens.copilotToken = token;
      
      std::wstring expiresStr = extractJsonValue(httpResponse.body, L"expires_at");
      if (!expiresStr.empty()) {
        tokens.copilotTokenExpires = static_cast<time_t>(std::stoll(expiresStr));
      } else {
        tokens.copilotTokenExpires = time(nullptr) + 1800;
      }
      
      tokens.isAuthenticated = true;
      return true;
    }
  }

  setLastCopilotAuthDebug(buildAuthDebugMessage(httpResponse));
  
  return false;
}

LLMResponse LLMApiClient::callCopilot(CopilotTokens &tokens,
                                      const std::wstring &prompt,
                                      const std::wstring &model) {
  LLMResponse response;
  
  if (!tokens.isAuthenticated || tokens.oauthToken.empty()) {
    response.errorMessage = L"GitHub Copilot is not authenticated. Please sign in first.";
    return response;
  }
  
  if (time(nullptr) >= tokens.copilotTokenExpires - 60) {
    if (!refreshCopilotToken(tokens)) {
      response.errorMessage = L"Failed to refresh Copilot token. Please sign in again.";
      tokens.isAuthenticated = false;
      return response;
    }
  }
  
  std::wstring escapedPrompt = escapeJsonString(prompt);
  std::wstring requestBody =
      L"{\"model\":\"" + model +
      L"\","
      L"\"messages\":[{\"role\":\"user\",\"content\":\"" +
      escapedPrompt +
      L"\"}],"
      L"\"stream\":false}";
  
  std::map<std::wstring, std::wstring> headers;
  headers[L"Content-Type"] = L"application/json";
  headers[L"Authorization"] = L"Bearer " + tokens.copilotToken;
  headers[L"Editor-Version"] = L"Notepad++/1.0";
  headers[L"Editor-Plugin-Version"] = L"AIAssistant/1.0";
  headers[L"User-Agent"] = kGitHubUserAgent;
  
  HttpResponse httpResponse = HttpClient::post(
      L"https://api.githubcopilot.com/chat/completions", requestBody, headers);
  
  if (!httpResponse.success) {
    response.errorMessage = L"HTTP request failed: " + httpResponse.errorMessage;
    if (!httpResponse.body.empty()) {
      std::wstring errorMsg = extractJsonValue(httpResponse.body, L"message");
      if (!errorMsg.empty())
        response.errorMessage += L"\n" +
            sanitizeUntrustedProviderError(errorMsg, tokens.copilotToken);
    }
    response.failure = ResponseFailure::HttpError;
    return response;
  }
  
  applyEnvelopeResult(response, extractChatCompletionEnvelope(httpResponse.body),
                      {}, tokens.copilotToken);
  
  return response;
}

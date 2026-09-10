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

#include "HttpClient.h"
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

// Static member initialization
DWORD HttpClient::_timeoutMs = HttpClient::DEFAULT_TIMEOUT_MS;
thread_local HttpClient::TransportObserver HttpClient::_threadTransportObserver;

void HttpClient::setThreadTransportObserver(TransportObserver observer) {
  _threadTransportObserver = std::move(observer);
}

void HttpClient::notifyTransportPhase(HttpTransportPhase phase) {
  if (_threadTransportObserver) _threadTransportObserver(phase);
}

std::wstring HttpClient::utf8ToWide(const std::string &utf8) {
  if (utf8.empty())
    return L"";
  if (utf8.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
    return L"";

  int wideLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                    utf8.data(), static_cast<int>(utf8.size()),
                                    nullptr, 0);
  if (wideLen <= 0)
    return L"";

  std::wstring wide(static_cast<size_t>(wideLen), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                          static_cast<int>(utf8.size()),
                          &wide[0], wideLen) <= 0) {
    return L"";
  }
  return wide;
}

std::string HttpClient::wideToUtf8(const std::wstring &wide) {
  if (wide.empty())
    return "";
  if (wide.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
    return "";

  int utf8Len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                    wide.data(), static_cast<int>(wide.size()),
                                    nullptr, 0, nullptr, nullptr);
  if (utf8Len <= 0)
    return "";

  std::string utf8(static_cast<size_t>(utf8Len), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
                          static_cast<int>(wide.size()),
                          &utf8[0], utf8Len, nullptr, nullptr) <= 0) {
    return "";
  }
  return utf8;
}

std::wstring HttpClient::sanitizeUrl(const std::wstring &url) {
  std::wstring sanitized = url;

  const std::wstring keyMarker = L"key=";
  size_t markerPos = sanitized.find(keyMarker);
  while (markerPos != std::wstring::npos) {
    size_t valueStart = markerPos + keyMarker.size();
    size_t valueEnd = sanitized.find_first_of(L"&#", valueStart);
    if (valueEnd == std::wstring::npos) {
      valueEnd = sanitized.size();
    }
    sanitized.replace(valueStart, valueEnd - valueStart, L"<redacted>");
    markerPos = sanitized.find(keyMarker, valueStart + 10);
  }

  return sanitized;
}

bool HttpClient::parseUrl(const std::wstring &url, std::wstring &host,
                          std::wstring &path, INTERNET_PORT &port,
                          bool &isHttps) {
  URL_COMPONENTS urlComp = {};
  urlComp.dwStructSize = sizeof(urlComp);

  wchar_t hostBuffer[256] = {};
  wchar_t pathBuffer[1024] = {};

  urlComp.lpszHostName = hostBuffer;
  urlComp.dwHostNameLength = 256;
  urlComp.lpszUrlPath = pathBuffer;
  urlComp.dwUrlPathLength = 1024;

  if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.length()), 0,
                       &urlComp)) {
    return false;
  }

  host = hostBuffer;
  path = pathBuffer;
  port = urlComp.nPort;
  isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

  if (path.empty())
    path = L"/";

  return true;
}

HttpResponse
HttpClient::post(const std::wstring &url, const std::wstring &body,
                 const std::map<std::wstring, std::wstring> &headers,
                 DWORD timeoutMs, bool bypassProxy) {
  HttpResponse response;
  const std::string utf8Body = wideToUtf8(body);
  if (!body.empty() && utf8Body.empty()) {
    response.errorMessage = L"HTTP request body contains invalid Unicode";
    notifyTransportPhase(HttpTransportPhase::Failed);
    return response;
  }

  // Parse URL
  std::wstring host, path;
  INTERNET_PORT port;
  bool isHttps;

  if (!parseUrl(url, host, path, port, isHttps)) {
    response.errorMessage = L"Failed to parse URL: " + sanitizeUrl(url);
    notifyTransportPhase(HttpTransportPhase::Failed);
    return response;
  }

  // Initialize WinHTTP session
  HINTERNET hSession = WinHttpOpen(
      L"Notepad++ AI Assistant/1.0", bypassProxy ? WINHTTP_ACCESS_TYPE_NO_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

  if (!hSession) {
    response.errorMessage = L"Failed to open WinHTTP session";
    notifyTransportPhase(HttpTransportPhase::Failed);
    return response;
  }

  // Set timeouts
  const DWORD requestTimeout = timeoutMs == 0 ? _timeoutMs : timeoutMs;
  WinHttpSetTimeouts(hSession, requestTimeout, requestTimeout, requestTimeout, requestTimeout);

  // Connect to server. This is transport progress, not a claim that the model
  // service is healthy; a valid response is required for that conclusion.
  notifyTransportPhase(HttpTransportPhase::Connecting);
  HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
  if (!hConnect) {
    response.errorMessage = L"Failed to connect to: " + host;
    WinHttpCloseHandle(hSession);
    notifyTransportPhase(HttpTransportPhase::Failed);
    return response;
  }

  // Create request
  DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
                                          nullptr, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

  if (!hRequest) {
    response.errorMessage = L"Failed to create HTTP request";
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    notifyTransportPhase(HttpTransportPhase::Failed);
    return response;
  }

  DWORD disabledFeatures = WINHTTP_DISABLE_REDIRECTS;
  if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &disabledFeatures,
                        sizeof(disabledFeatures))) {
    response.errorMessage = L"Failed to disable HTTP redirects";
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    notifyTransportPhase(HttpTransportPhase::Failed);
    return response;
  }

  // Add headers
  for (const auto &header : headers) {
    std::wstring headerLine = header.first + L": " + header.second;
    WinHttpAddRequestHeaders(hRequest, headerLine.c_str(),
                             static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD);
  }

  // Send request
  notifyTransportPhase(HttpTransportPhase::Sending);
  BOOL sendResult = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS,
                                       0, (LPVOID)utf8Body.c_str(),
                                       static_cast<DWORD>(utf8Body.size()),
                                       static_cast<DWORD>(utf8Body.size()), 0);

  if (!sendResult) {
    DWORD error = GetLastError();
    response.errorMessage =
        L"Failed to send request. Error code: " + std::to_wstring(error);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    notifyTransportPhase(HttpTransportPhase::ConnectionFailed);
    return response;
  }

  // Receive response
  notifyTransportPhase(HttpTransportPhase::AwaitingResponse);
  if (!WinHttpReceiveResponse(hRequest, nullptr)) {
    DWORD error = GetLastError();
    response.errorMessage =
        L"Failed to receive response. Error code: " + std::to_wstring(error);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    notifyTransportPhase(HttpTransportPhase::ConnectionFailed);
    return response;
  }

  // Get status code
  DWORD statusCode = 0;
  DWORD statusCodeSize = sizeof(statusCode);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                      &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
  response.statusCode = statusCode;

  // Read response body
  notifyTransportPhase(HttpTransportPhase::Receiving);
  std::string responseBody;
  const size_t maxResponseBytes = bypassProxy ? 1024 * 1024 : 4 * 1024 * 1024;
  DWORD bytesAvailable = 0;

  do {
    bytesAvailable = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
      response.errorMessage =
          L"Failed to query HTTP response data. Error code: " +
          std::to_wstring(GetLastError());
      break;
    }

    if (bytesAvailable == 0)
      break;

    if (responseBody.size() + bytesAvailable > maxResponseBytes) {
      response.errorMessage = L"HTTP response exceeded bounded size limit";
      break;
    }

    std::vector<char> buffer(bytesAvailable + 1, 0);
    DWORD bytesRead = 0;

    if (!WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
      response.errorMessage =
          L"Failed to read HTTP response data. Error code: " +
          std::to_wstring(GetLastError());
      break;
    }
    if (bytesRead == 0) {
      response.errorMessage = L"HTTP response body ended before completion";
      break;
    }
    responseBody.append(buffer.data(), bytesRead);
  } while (bytesAvailable > 0);

  // Convert response to wide string
  response.body = utf8ToWide(responseBody);
  if (!responseBody.empty() && response.body.empty()) {
    response.errorMessage = L"HTTP response contains invalid UTF-8";
  }
  response.success = response.errorMessage.empty() && statusCode >= 200 && statusCode < 300;
  notifyTransportPhase(response.success ? HttpTransportPhase::Completed
                                        : HttpTransportPhase::Failed);

  // Cleanup
  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);

  return response;
}

HttpResponse HttpClient::get(const std::wstring &url) {
  std::map<std::wstring, std::wstring> emptyHeaders;
  return get(url, emptyHeaders);
}

HttpResponse HttpClient::get(const std::wstring &url,
                             const std::map<std::wstring, std::wstring> &headers,
                             DWORD timeoutMs, bool bypassProxy) {
  HttpResponse response;

  std::wstring host, path;
  INTERNET_PORT port;
  bool isHttps;

  if (!parseUrl(url, host, path, port, isHttps)) {
    response.errorMessage = L"Failed to parse URL: " + sanitizeUrl(url);
    notifyTransportPhase(HttpTransportPhase::Failed);
    return response;
  }

  HINTERNET hSession = WinHttpOpen(
      L"Notepad++ AI Assistant/1.0", bypassProxy ? WINHTTP_ACCESS_TYPE_NO_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

  if (!hSession) {
    response.errorMessage = L"Failed to open WinHTTP session";
    notifyTransportPhase(HttpTransportPhase::Failed);
    return response;
  }

  const DWORD requestTimeout = timeoutMs == 0 ? _timeoutMs : timeoutMs;
  WinHttpSetTimeouts(hSession, requestTimeout, requestTimeout, requestTimeout, requestTimeout);

  notifyTransportPhase(HttpTransportPhase::Connecting);
  HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
  if (!hConnect) {
    response.errorMessage = L"Failed to connect to: " + host;
    WinHttpCloseHandle(hSession);
    notifyTransportPhase(HttpTransportPhase::Failed);
    return response;
  }

  DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                          nullptr, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

  if (!hRequest) {
    response.errorMessage = L"Failed to create HTTP request";
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    notifyTransportPhase(HttpTransportPhase::Failed);
    return response;
  }

  DWORD disabledFeatures = WINHTTP_DISABLE_REDIRECTS;
  if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &disabledFeatures,
                        sizeof(disabledFeatures))) {
    response.errorMessage = L"Failed to disable HTTP redirects";
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    notifyTransportPhase(HttpTransportPhase::Failed);
    return response;
  }

  for (const auto &header : headers) {
    std::wstring headerLine = header.first + L": " + header.second;
    WinHttpAddRequestHeaders(hRequest, headerLine.c_str(),
                             static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD);
  }

  notifyTransportPhase(HttpTransportPhase::Sending);
  if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    response.errorMessage = L"Failed to send GET request";
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    notifyTransportPhase(HttpTransportPhase::ConnectionFailed);
    return response;
  }

  notifyTransportPhase(HttpTransportPhase::AwaitingResponse);
  if (!WinHttpReceiveResponse(hRequest, nullptr)) {
    response.errorMessage = L"Failed to receive response";
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    notifyTransportPhase(HttpTransportPhase::ConnectionFailed);
    return response;
  }

  DWORD statusCode = 0;
  DWORD statusCodeSize = sizeof(statusCode);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                      &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
  response.statusCode = statusCode;

  notifyTransportPhase(HttpTransportPhase::Receiving);
  std::string responseBody;
  const size_t maxResponseBytes = bypassProxy ? 1024 * 1024 : 4 * 1024 * 1024;
  DWORD bytesAvailable = 0;

  do {
    bytesAvailable = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
      response.errorMessage =
          L"Failed to query HTTP response data. Error code: " +
          std::to_wstring(GetLastError());
      break;
    }

    if (bytesAvailable == 0)
      break;

    if (responseBody.size() + bytesAvailable > maxResponseBytes) {
      response.errorMessage = L"HTTP response exceeded bounded size limit";
      break;
    }

    std::vector<char> buffer(bytesAvailable + 1, 0);
    DWORD bytesRead = 0;

    if (!WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
      response.errorMessage =
          L"Failed to read HTTP response data. Error code: " +
          std::to_wstring(GetLastError());
      break;
    }
    if (bytesRead == 0) {
      response.errorMessage = L"HTTP response body ended before completion";
      break;
    }
    responseBody.append(buffer.data(), bytesRead);
  } while (bytesAvailable > 0);

  response.body = utf8ToWide(responseBody);
  if (!responseBody.empty() && response.body.empty()) {
    response.errorMessage = L"HTTP response contains invalid UTF-8";
  }
  response.success = response.errorMessage.empty() && statusCode >= 200 && statusCode < 300;
  notifyTransportPhase(response.success ? HttpTransportPhase::Completed
                                        : HttpTransportPhase::Failed);

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);

  return response;
}

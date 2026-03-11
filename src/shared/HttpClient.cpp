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
#include <sstream>
#include <stdexcept>
#include <vector>

// Static member initialization
DWORD HttpClient::_timeoutMs = HttpClient::DEFAULT_TIMEOUT_MS;

std::wstring HttpClient::utf8ToWide(const std::string &utf8) {
  if (utf8.empty())
    return L"";

  int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
  if (wideLen <= 0)
    return L"";

  std::wstring wide(wideLen - 1, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], wideLen);
  return wide;
}

std::string HttpClient::wideToUtf8(const std::wstring &wide) {
  if (wide.empty())
    return "";

  int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0,
                                    nullptr, nullptr);
  if (utf8Len <= 0)
    return "";

  std::string utf8(utf8Len - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], utf8Len, nullptr,
                      nullptr);
  return utf8;
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
                 const std::map<std::wstring, std::wstring> &headers) {
  HttpResponse response;

  // Parse URL
  std::wstring host, path;
  INTERNET_PORT port;
  bool isHttps;

  if (!parseUrl(url, host, path, port, isHttps)) {
    response.errorMessage = L"Failed to parse URL: " + url;
    return response;
  }

  // Initialize WinHTTP session
  HINTERNET hSession = WinHttpOpen(
      L"Notepad++ AI Assistant/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

  if (!hSession) {
    response.errorMessage = L"Failed to open WinHTTP session";
    return response;
  }

  // Set timeouts
  WinHttpSetTimeouts(hSession, _timeoutMs, _timeoutMs, _timeoutMs, _timeoutMs);

  // Connect to server
  HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
  if (!hConnect) {
    response.errorMessage = L"Failed to connect to: " + host;
    WinHttpCloseHandle(hSession);
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
    return response;
  }

  // Add headers
  for (const auto &header : headers) {
    std::wstring headerLine = header.first + L": " + header.second;
    WinHttpAddRequestHeaders(hRequest, headerLine.c_str(),
                             static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD);
  }

  // Convert body to UTF-8
  std::string utf8Body = wideToUtf8(body);

  // Send request
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
    return response;
  }

  // Receive response
  if (!WinHttpReceiveResponse(hRequest, nullptr)) {
    DWORD error = GetLastError();
    response.errorMessage =
        L"Failed to receive response. Error code: " + std::to_wstring(error);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
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
  std::string responseBody;
  DWORD bytesAvailable = 0;

  do {
    bytesAvailable = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable))
      break;

    if (bytesAvailable == 0)
      break;

    std::vector<char> buffer(bytesAvailable + 1, 0);
    DWORD bytesRead = 0;

    if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
      responseBody.append(buffer.data(), bytesRead);
    }
  } while (bytesAvailable > 0);

  // Convert response to wide string
  response.body = utf8ToWide(responseBody);
  response.success = (statusCode >= 200 && statusCode < 300);

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
                             const std::map<std::wstring, std::wstring> &headers) {
  HttpResponse response;

  std::wstring host, path;
  INTERNET_PORT port;
  bool isHttps;

  if (!parseUrl(url, host, path, port, isHttps)) {
    response.errorMessage = L"Failed to parse URL: " + url;
    return response;
  }

  HINTERNET hSession = WinHttpOpen(
      L"Notepad++ AI Assistant/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

  if (!hSession) {
    response.errorMessage = L"Failed to open WinHTTP session";
    return response;
  }

  WinHttpSetTimeouts(hSession, _timeoutMs, _timeoutMs, _timeoutMs, _timeoutMs);

  HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
  if (!hConnect) {
    response.errorMessage = L"Failed to connect to: " + host;
    WinHttpCloseHandle(hSession);
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
    return response;
  }

  for (const auto &header : headers) {
    std::wstring headerLine = header.first + L": " + header.second;
    WinHttpAddRequestHeaders(hRequest, headerLine.c_str(),
                             static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD);
  }

  if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    response.errorMessage = L"Failed to send GET request";
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
  }

  if (!WinHttpReceiveResponse(hRequest, nullptr)) {
    response.errorMessage = L"Failed to receive response";
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
  }

  DWORD statusCode = 0;
  DWORD statusCodeSize = sizeof(statusCode);
  WinHttpQueryHeaders(hRequest,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                      &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
  response.statusCode = statusCode;

  std::string responseBody;
  DWORD bytesAvailable = 0;

  do {
    bytesAvailable = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable))
      break;

    if (bytesAvailable == 0)
      break;

    std::vector<char> buffer(bytesAvailable + 1, 0);
    DWORD bytesRead = 0;

    if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
      responseBody.append(buffer.data(), bytesRead);
    }
  } while (bytesAvailable > 0);

  response.body = utf8ToWide(responseBody);
  response.success = (statusCode >= 200 && statusCode < 300);

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);

  return response;
}

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
#include <map>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

// HTTP response structure
struct HttpResponse {
    DWORD statusCode = 0;
    std::wstring body;
    bool success = false;
    std::wstring errorMessage;
};

// Request-local transport controls.  The caller owns this immutable snapshot;
// keeping it per request prevents settings changes from affecting in-flight work.
struct HttpRequestOptions {
    DWORD timeoutMs = 30000;
    bool bypassProxy = false;
    bool disableRedirects = false;
    size_t maxResponseBytes = 4 * 1024 * 1024;
};

// Simple HTTP client wrapper using WinHTTP
class HttpClient {
public:
    // POST request with JSON body
    static HttpResponse post(
        const std::wstring& url,
        const std::wstring& body,
        const std::map<std::wstring, std::wstring>& headers,
        const HttpRequestOptions& options = HttpRequestOptions{}
    );
    
    // GET request
    static HttpResponse get(const std::wstring& url,
                            const HttpRequestOptions& options = HttpRequestOptions{});
    
    // GET request with headers
    static HttpResponse get(
        const std::wstring& url,
        const std::map<std::wstring, std::wstring>& headers,
        const HttpRequestOptions& options = HttpRequestOptions{}
    );
    
private:
    // Parse URL into components
    static bool parseUrl(
        const std::wstring& url,
        std::wstring& host,
        std::wstring& path,
        INTERNET_PORT& port,
        bool& isHttps
    );

    static std::wstring sanitizeUrl(const std::wstring& url);
    
    // Convert UTF-8 to wide string
    static std::wstring utf8ToWide(const std::string& utf8);
    
    // Convert wide string to UTF-8
    static std::string wideToUtf8(const std::wstring& wide);
};

#include "LLMApiClient.h"
#include "HttpClient.h"
#include <cstdlib>
#include <iostream>

namespace {
HttpResponse nextResponse;
DWORD lastTimeoutMs = 0;
bool lastDirectConnection = false;
void expect(bool passed, const char *message) {
  if (!passed) { std::cerr << message << '\n'; std::exit(1); }
}
}

// Exercise the real provider error path without a network or a real key.
HttpResponse HttpClient::post(const std::wstring &, const std::wstring &,
    const std::map<std::wstring, std::wstring> &, DWORD timeoutMs, bool direct) {
  lastTimeoutMs = timeoutMs;
  lastDirectConnection = direct;
  return nextResponse;
}
HttpResponse HttpClient::get(const std::wstring &) { return nextResponse; }
HttpResponse HttpClient::get(const std::wstring &,
    const std::map<std::wstring, std::wstring> &, DWORD, bool) { return nextResponse; }

int main() {
  for (DWORD status : {400UL, 401UL, 404UL, 422UL, 429UL, 500UL, 503UL}) {
    nextResponse = {};
    nextResponse.statusCode = status;
    nextResponse.body = L"{\"error\":{\"message\":\"synthetic-secret and private-prompt\"}}";
    const auto response = LLMApiClient::callOpenAICompatible(
        L"http://127.0.0.1:1/v1", L"synthetic-secret", L"private-prompt",
        L"test-model", CompatibleApiMode::ChatCompletions, true);
    expect(!response.success && response.failure == ResponseFailure::HttpError, "HTTP failure must remain typed");
    expect(response.errorMessage.find(std::to_wstring(status)) != std::wstring::npos, "HTTP status must be visible");
    expect(response.errorMessage.find(L"synthetic-secret") == std::wstring::npos &&
           response.errorMessage.find(L"private-prompt") == std::wstring::npos,
           "Provider body must not leak into error diagnostics");
  }
  nextResponse = {};
  nextResponse.errorMessage = L"Failed to receive response. Error code: 12002";
  const auto timeout = LLMApiClient::callOpenAICompatible(
      L"http://127.0.0.1:1/v1", L"", L"test", L"test-model",
      CompatibleApiMode::ChatCompletions, true);
  expect(timeout.errorMessage.find(L"12002") != std::wstring::npos, "Transport error must remain distinguishable");
  for (const auto mode : {CompatibleApiMode::ChatCompletions, CompatibleApiMode::Responses}) {
    LLMApiClient::callOpenAICompatible(
        L"http://127.0.0.1:1/v1", L"", L"test", L"test-model", mode, true);
    expect(lastTimeoutMs == 900000 && lastDirectConnection,
           "Local generation must use a 900-second direct request");
    LLMApiClient::callOpenAICompatible(
        L"https://example.com/v1", L"", L"test", L"test-model", mode, false);
    expect(lastTimeoutMs == 0 && !lastDirectConnection,
           "Remote generation must retain the default transport timeout");
  }
  std::cout << "PASS: HTTP statuses and transport errors are visible without response payloads\n";
}

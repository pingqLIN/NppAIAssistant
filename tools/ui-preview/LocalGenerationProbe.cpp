// Explicit, synthetic loopback smoke test; never runs automatically in CTest.
#include "LLMApiClient.h"
#include <chrono>
#include <iostream>
int wmain(int argc, wchar_t **argv) {
  if (argc != 2) return 2;
  const auto start = std::chrono::steady_clock::now();
  const auto reply = LLMApiClient::callOpenAICompatible(
      L"http://127.0.0.1:1234/v1", L"", L"Reply with exactly OK.", argv[1],
      CompatibleApiMode::ChatCompletions, true);
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start).count();
  std::cout << "success=" << reply.success << " nonempty=" << !reply.content.empty()
            << " elapsedMs=" << elapsed << '\n';
  return reply.success && !reply.content.empty() ? 0 : 1;
}

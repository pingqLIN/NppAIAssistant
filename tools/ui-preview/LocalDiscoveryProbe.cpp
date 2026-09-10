#include "LLMApiClient.h"
#include <iostream>

int main() {
  const auto models = LLMApiClient::listOpenAICompatibleModels(
      L"http://127.0.0.1:1234/v1", L"", true);
  std::cout << "Local discovery success=" << models.success
            << " modelCount=" << models.models.size() << '\n';
  return models.success && !models.models.empty() ? 0 : 1;
}

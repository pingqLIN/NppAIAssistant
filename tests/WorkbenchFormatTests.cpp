#include "WorkbenchFormat.h"
#include "StructuredOutput.h"
#include <cstdlib>
#include <iostream>
void check(bool value) { if (!value) std::exit(1); }
int main() {
  using namespace WorkbenchFormat;
  auto rendered = markdown(L"# 標題\r\n**bold** and `code`\n- item\n> quote\n```cpp\n  **literal**\n```");
  check(rendered.text.find(L"標題\r") == 0);
  check(rendered.text.find(L"bold and code") != std::wstring::npos);
  check(rendered.text.find(L"• item") != std::wstring::npos);
  check(rendered.text.find(L"  **literal**") != std::wstring::npos);
  for (auto span : rendered.spans) check(span.start + span.length <= rendered.text.size());
  check(markdown(L"<script>x</script> [link](https://example.com)").text.find(L"<script>") == 0);
  check(markdown(L"unclosed **marker").text == L"unclosed **marker\r");
  check(richLines(L"a\r\nb\nc\r") == L"a\rb\rc\r");
  std::wstring pretty;
  check(prettyPrintJson(L"{\"n\":9007199254740993,\"s\":\"a\\\"b\",\"v\":[true,null]}", pretty));
  check(pretty.find(L"9007199254740993") != std::wstring::npos);
  check(pretty.find(L"a\\\"b") != std::wstring::npos);
  std::wstring again; check(prettyPrintJson(pretty, again) && again == pretty);
  check(!prettyPrintJson(L"{\"x\":}", pretty) && pretty.empty());
  check(!prettyPrintJson(L"{\"x\":1} trailing", pretty));
  check(animate(true, false, 100, true));
  check(!animate(true, false, 0, false));
  check(animate(false, true, 7, true) && !animate(false, true, 8, true));
  check(std::wstring(frame(true, false, false, false, 0)) != frame(true, false, false, false, 1));
  check(std::wstring(frame(false, true, false, false, 8)) != frame(false, false, true, false, 8));
  std::cout << "Markdown, JSON and status animation scenarios passed\n";
}

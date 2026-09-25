#pragma once
#include <string>
#include <vector>
#include <sstream>

namespace WorkbenchFormat {
enum class Style { Heading, Bold, Code, Quote };
struct Span { size_t start, length; Style style; };
struct Rendered { std::wstring text; std::vector<Span> spans; };
inline std::wstring richLines(const std::wstring &source) {
  std::wstring result;
  for (size_t i = 0; i < source.size(); ++i) {
    if (source[i] == L'\r' && i + 1 < source.size() && source[i + 1] == L'\n') ++i;
    result += source[i] == L'\n' ? L'\r' : source[i];
  }
  return result;
}
// A deliberately small text-only Markdown renderer. Never executes HTML or
// fetches links/images. Raw model content remains separate for editor writes.
inline Rendered markdown(const std::wstring &source) {
  Rendered result;
  std::wstring normalized = richLines(source);
  std::wistringstream input(normalized);
  std::wstring line;
  bool code = false;
  while (std::getline(input, line, L'\r')) {
    if (line.rfind(L"```", 0) == 0) { code = !code; continue; }
    const size_t start = result.text.size();
    Style lineStyle = Style::Bold;
    bool styledLine = code;
    if (code) lineStyle = Style::Code;
    else if (line.rfind(L"> ", 0) == 0) { line.erase(0, 2); styledLine = true; lineStyle = Style::Quote; }
    else {
      size_t marks = 0;
      while (marks < line.size() && line[marks] == L'#') ++marks;
      if (marks > 0 && marks <= 6 && marks < line.size() && line[marks] == L' ') {
        line.erase(0, marks + 1); styledLine = true; lineStyle = Style::Heading;
      }
      if (line.rfind(L"- ", 0) == 0 || line.rfind(L"* ", 0) == 0) line.replace(0, 2, L"• ");
    }
    for (size_t i = 0; i < line.size();) {
      const bool bold = !code && line.compare(i, 2, L"**") == 0;
      const bool inlineCode = !code && line[i] == L'`';
      const size_t delimiter = bold ? 2 : 1;
      const auto end = (bold || inlineCode)
          ? line.find(bold ? L"**" : L"`", i + delimiter) : std::wstring::npos;
      if (end != std::wstring::npos && end > i + delimiter) {
        const auto offset = result.text.size();
        result.text += line.substr(i + delimiter, end - i - delimiter);
        result.spans.push_back({offset, result.text.size() - offset, bold ? Style::Bold : Style::Code});
        i = end + delimiter;
      } else result.text += line[i++];
    }
    if (styledLine) result.spans.push_back({start, result.text.size() - start, lineStyle});
    result.text += L'\r';
  }
  return result;
}
// Terminal states animate briefly, then remain visible. Respect reduced motion.
inline bool animate(bool busy, bool terminal, unsigned tick, bool motion) {
  return motion && (busy || (terminal && tick < 8));
}
inline const wchar_t *frame(bool busy, bool disconnected, bool failed, bool success, unsigned tick) {
  constexpr const wchar_t *spinner[] = {L"◐", L"◓", L"◑", L"◒"};
  if (busy) return spinner[tick % 4];
  if (disconnected) return tick < 8 && tick % 2 ? L"○" : L"⊘";
  if (failed) return tick < 8 && tick % 2 ? L"△" : L"⚠";
  return success ? L"✓" : L"●";
}
}

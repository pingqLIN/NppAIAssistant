#pragma once

#include <algorithm>
#include <array>
#include <utility>

namespace PanelLayout {

enum Control {
  Title, Settings, Provider, Model, Profile, Output, Preview, NewConversation,
  Branch, FontPlus, FontMinus, Order, Status, History, Splitter, Input,
  Hint, Send, Destination, Reply, InsertReply, ViewMode, Count
};

struct Rect {
  int x = 0, y = 0, width = 0, height = 0;
  bool visible() const { return width > 0 && height > 0; }
};

struct Metrics {
  int margin = 12, gap = 8, row = 30, text = 22;
  int settings = 76, preview = 160, newConversation = 64, branch = 60;
  int insertReply = 120;
  int send = 88, font = 32, preferredInput = 88;
};

// All geometry is computed before touching HWNDs. Collapsed panels are valid:
// never give a child a negative extent or let a control cover another row.
inline std::array<Rect, Count> compute(int width, int height, Metrics m) {
  std::array<Rect, Count> r{};
  width = std::max(0, width);
  height = std::max(0, height);
  const int inset = std::min(m.margin, width / 4);
  const int content = std::max(0, width - 2 * inset);
  const int bottom = std::max(0, height - m.margin);
  int y = std::min(m.margin, bottom);
  const auto put = [&](Control id, int x, int top, int w, int h) {
    if (w > 0 && h > 0 && x >= 0 && top >= 0 && x + w <= width &&
        top + h <= bottom) r[id] = {x, top, w, h};
  };
  const bool stackedSend = content < m.preview + m.send + m.gap;
  const int footer = (stackedSend ? 2 * m.row + m.gap : m.row) + m.text + 2 * m.gap;
  const int minimumBody = m.row * 4 + footer + 2 * (m.row + m.gap);
  const auto roomForRow = [&] { return bottom - y >= minimumBody + m.row + m.gap; };
  if (roomForRow()) {
    const int settings = std::min(content, m.settings);
    put(Title, inset, y, content - settings - m.gap, m.row);
    put(Settings, width - inset - settings, y, settings, m.row);
    y += m.row + m.gap;
  }
  // Full-width selectors on a narrow dock keep model names discoverable.
  if (roomForRow()) {
    const bool twoColumns = content >= 16 * m.row;
    const int provider = twoColumns ? (content - m.gap) * 2 / 5 : content;
    put(Provider, inset, y, provider, m.row);
    if (twoColumns) {
      put(Model, inset + provider + m.gap, y,
          content - provider - m.gap, m.row);
    }
    y += m.row + m.gap;
    if (!twoColumns && roomForRow()) {
      put(Model, inset, y, content, m.row);
      y += m.row + m.gap;
    }
  }
  if (roomForRow()) {
    const int first = std::max(0, (content - m.gap) / 2);
    put(Profile, inset, y, first, m.row);
    put(Output, inset + first + m.gap, y, content - first - m.gap, m.row);
    y += m.row + m.gap;
  }
  if (roomForRow()) {
    int x = inset;
    for (const auto item : {std::pair{NewConversation, m.newConversation},
                            std::pair{Branch, m.branch}}) {
      if (x + item.second <= width - inset) {
        put(item.first, x, y, item.second, m.row);
        x += item.second + m.gap;
      }
    }
    if (x + 2 * m.font + m.gap <= width - inset) {
      put(FontMinus, x, y, m.font, m.row);
      put(FontPlus, x + m.font + m.gap, y, m.font, m.row);
    }
    y += m.row + m.gap;
  }
  if (roomForRow()) {
    put(ViewMode, inset, y, content, m.row);
    y += m.row + m.gap;
  }
  if (bottom - y > minimumBody + m.text) {
    put(Status, inset, y, content, m.text);
    y += m.text + m.gap;
  }
  const int sendY = bottom - m.row;
  const int previewY = stackedSend ? sendY - m.row - m.gap : sendY;
  const int destinationY = previewY - m.row - m.gap;
  const int hintY = destinationY - m.text - m.gap;
  const int body = std::max(0, hintY - m.gap - y - m.row - m.gap);
  const int divider = std::min(m.gap, body);
  const int inputHeight = std::clamp(m.preferredInput, 0,
                                    std::max(0, (body - divider) * 2 / 3));
  const int historyHeight = std::max(0, body - divider - inputHeight);
  put(History, inset, y, content, historyHeight);
  const int replyY = y + historyHeight;
  const int replyButton = std::min(content, m.insertReply);
  if (replyY + m.row <= hintY - m.gap) {
    put(Reply, inset, replyY, content - replyButton - m.gap, m.row);
    put(InsertReply, width - inset - replyButton, replyY, replyButton, m.row);
  }
  const int splitY = replyY + m.row + m.gap;
  put(Splitter, inset, splitY, content, divider);
  put(Input, inset, splitY + divider, content, inputHeight);
  const int sendWidth = std::min(content, m.send);
  put(Send, width - inset - sendWidth, sendY, sendWidth, m.row);
  put(Preview, inset, previewY, std::min(content, m.preview), m.row);
  put(Hint, inset, hintY, content, m.text);
  put(Destination, inset, destinationY, content, m.row);
  return r;
}

// A physical Enter press produces both key and character messages on Win32.
// Dispatch once and consume its translated CR/LF, including auto-repeat.
class EnterShortcut {
public:
  bool keyDown(bool requireCtrl, bool ctrl, bool shift, bool alt,
               bool composing, bool repeat) {
    if (!repeat) consume_ = !composing && !alt &&
                           (requireCtrl ? ctrl && !shift : !shift);
    return consume_ && !repeat;
  }
  bool consumesCharacter() const { return consume_; }
  void reset() { consume_ = false; }
private:
  bool consume_ = false;
};

} // namespace PanelLayout

#include "PanelLayout.h"

#include <cstdlib>
#include <iostream>

void expect(bool value, const char *reason) {
  if (!value) { std::cerr << reason << '\n'; std::exit(1); }
}

int main(int argc, char **argv) {
  using namespace PanelLayout;
  if (argc == 4 && std::string(argv[1]) == "--layout") {
    const auto rects = compute(std::atoi(argv[2]), std::atoi(argv[3]), Metrics{});
    std::cout << "[";
    for (int i = 0; i < Count; ++i) {
      if (i) std::cout << ",";
      const auto &r = rects[i];
      std::cout << "[" << r.x << "," << r.y << "," << r.width << "," << r.height << "]";
    }
    std::cout << "]\n";
    return 0;
  }
  int cases = 0;
  for (int percent : {100, 125, 150, 200, 300}) {
    const auto scale = [percent](int value) { return (value * percent + 50) / 100; };
    Metrics m;
    m.margin = scale(m.margin); m.gap = scale(m.gap); m.row = scale(m.row); m.text = scale(m.text);
    m.preview = scale(m.preview); m.newConversation = scale(m.newConversation); m.branch = scale(m.branch);
    m.insertReply = scale(m.insertReply); m.send = scale(m.send); m.font = scale(m.font); m.settings = scale(m.settings);
    for (int width : {0, 1, 48, 180, 240, 320, 480, 640, 1000}) {
      for (int height : {0, 1, 48, 160, 320, 600, 900}) {
        for (int preferred : {-100, 0, 88, 10000}) {
          m.preferredInput = preferred;
          const auto r = compute(scale(width), scale(height), m);
          expect(r[FontPlus].visible() == r[FontMinus].visible(), "font controls must remain a pair");
          for (int i = 0; i < Count; ++i) {
            const auto &a = r[i];
            expect(a.width >= 0 && a.height >= 0, "negative geometry");
            if (!a.visible()) continue;
            expect(a.x >= 0 && a.y >= 0 && a.x + a.width <= scale(width) &&
                   a.y + a.height <= scale(height), "control outside panel");
            for (int j = i + 1; j < Count; ++j) {
              const auto &b = r[j];
              if (!b.visible()) continue;
              expect(a.x + a.width <= b.x || b.x + b.width <= a.x ||
                     a.y + a.height <= b.y || b.y + b.height <= a.y,
                     "overlapping controls");
            }
          }
          ++cases;
        }
      }
    }
  }
  const auto narrow = compute(320, 800, Metrics{});
  expect(narrow[Provider].y < narrow[Model].y, "narrow selectors must stack");
  expect(narrow[Preview].visible(), "preview must remain accessible in narrow panel");
  expect(narrow[Preview].y >= narrow[Input].y + narrow[Input].height,
         "preview toggle must stay beside the composer, not in a secondary toolbar");
  const auto shortPanel = compute(240, 320, Metrics{});
  expect(shortPanel[Preview].visible() && shortPanel[Send].visible(),
         "short narrow panels must retain preview and send");
  expect(shortPanel[Preview].y < shortPanel[Send].y,
         "preview must stack above send when labels do not fit together");
  expect(narrow[Input].width == narrow[History].width, "composer must use full width");
  expect(narrow[Send].y >= narrow[Input].y + narrow[Input].height,
         "send must be below composer");
  const auto wide = compute(800, 800, Metrics{});
  expect(wide[Provider].y == wide[Model].y, "wide selectors must share a row");
  expect(wide[Preview].y == wide[Send].y, "wide panels place preview beside send");

  expect(narrow[Destination].visible() && narrow[Reply].visible() && narrow[InsertReply].visible(),
         "editor output controls must be accessible");
  expect(narrow[Destination].y > narrow[Input].y && narrow[Destination].y < narrow[Preview].y,
         "destination must stay next to send controls");
  EnterShortcut enter;
  expect(enter.keyDown(true, true, false, false, false, false), "Ctrl+Enter must send");
  expect(enter.consumesCharacter(), "translated CR and LF must be consumed");
  expect(!enter.keyDown(true, true, false, false, false, true), "repeat must not send");
  enter.reset();
  expect(!enter.consumesCharacter(), "key release must reset consumption");
  expect(!enter.keyDown(true, false, false, false, false, false), "Enter inserts newline in Ctrl mode");
  expect(!enter.keyDown(false, false, true, false, false, false), "Shift+Enter inserts newline");
  expect(!enter.keyDown(false, false, false, true, false, false), "Alt+Enter must not send");
  expect(!enter.keyDown(false, false, false, false, true, false), "IME confirmation must not send");
  expect(enter.keyDown(false, false, false, false, false, false), "Enter mode must send");
  std::cout << cases << " geometry cases and keyboard scenarios passed\n";
}

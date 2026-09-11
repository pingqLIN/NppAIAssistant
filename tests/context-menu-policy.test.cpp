#include "../src/shared/ContextMenuPolicy.h"
#include <iostream>

int main() {
  struct Case {
    const char *name;
    bool enabled, keyboard, ctrl, selection, expected;
  };
  const Case cases[] = {
      {"ordinary selected-text right-click stays native", true, false, false, true, false},
      {"Ctrl + selected-text mouse menu opens AI", true, false, true, true, true},
      {"Ctrl without selection stays native", true, false, true, false, false},
      {"disabled AI gesture stays native", false, false, true, true, false},
      {"Shift+F10 / Menu key stays native", true, true, false, true, false},
      {"keyboard menu with Ctrl stays native", true, true, true, true, false},
      {"unselected ordinary mouse menu stays native", true, false, false, false, false},
      {"disabled keyboard menu stays native", false, true, true, true, false},
  };
  for (const auto &c : cases) {
    if (ContextMenuPolicy::shouldShowAiMenu(c.enabled, c.keyboard, c.ctrl,
                                          c.selection) != c.expected) {
      std::cerr << "FAIL: " << c.name << '\n';
      return 1;
    }
  }
  std::cout << "PASS: 8 context-menu behavior cases\n";
}

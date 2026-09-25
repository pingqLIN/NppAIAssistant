#include "EditorOutput.h"
#include <cstdlib>
#include <iostream>
using namespace EditorOutput;
void check(bool value) { if (!value) std::exit(1); }
int main() {
  Snapshot saved{1, 2, 65001, "before TARGET after"};
  auto current = saved;
  check(canWrite(saved, current, 7, 13, false));
  check(canWrite(saved, current, 7, 7, false));
  check(!canWrite(saved, current, 7, 13, true));
  current.bytes = "BEFORE TARGET after";
  check(!canWrite(saved, current, 7, 13, false)); // Same range, edit elsewhere.
  current = saved; current.bytes.insert(0, "shift ");
  check(!canWrite(saved, current, 7, 7, false)); // Cursor anchor moved.
  current = saved; current.buffer = 9;
  check(!canWrite(saved, current, 7, 13, false)); // Tab switched or reopened.
  current = saved; current.document = 9;
  check(!canWrite(saved, current, 7, 13, false));
  current = saved; current.codePage = 950;
  check(!canWrite(saved, current, 7, 13, false));
  check(!canWrite(saved, saved, 13, 7, false));
  check(!canWrite(saved, saved, 0, 1000, false));
  check(!canWrite({}, {}, 0, 0, false));
  Snapshot empty{3, 4, 65001, ""};
  check(canWrite(empty, empty, 0, 0, false));
  std::cout << "Editor target safety scenarios passed\n";
}

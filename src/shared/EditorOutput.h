#pragma once
#include <cstdint>
#include <string>

namespace EditorOutput {
enum class Destination { Panel, Insert, Replace, NewDocument };
struct Snapshot {
  uintptr_t buffer = 0, document = 0;
  int codePage = 0;
  std::string bytes;
};
// A position alone is not an anchor: preceding edits can move the intended text.
inline bool canWrite(const Snapshot &saved, const Snapshot &current,
                     size_t start, size_t end, bool readOnly) {
  return !readOnly && saved.buffer != 0 && saved.document != 0 &&
      saved.buffer == current.buffer && saved.document == current.document &&
      saved.codePage == current.codePage && saved.bytes == current.bytes &&
      start <= end && end <= current.bytes.size();
}
}

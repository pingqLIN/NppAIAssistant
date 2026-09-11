#pragma once

namespace ContextMenuPolicy {
// WM_CONTEXTMENU uses (-1, -1) coordinates for keyboard invocation.
constexpr bool shouldShowAiMenu(bool enabled, bool keyboardInvocation,
                                bool ctrlPressed, bool hasSelection) {
  return enabled && !keyboardInvocation && ctrlPressed && hasSelection;
}
} // namespace ContextMenuPolicy

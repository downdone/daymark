#pragma once
#include <functional>
class QWindow;
namespace daymark {
// Test fixture only: bounded cancellation in the platform's native menu loop.
// This does not synthesize pointer input or prove that a command was clicked.
bool scheduleSmokeMenuDismiss(QWindow*,std::function<void()> beforeDismiss);
}

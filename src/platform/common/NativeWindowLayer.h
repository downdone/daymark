#pragma once
#include "platform/IWindowPlatform.h"
class QWindow;
namespace daymark {
// Only native backends implement these; offscreen tests cannot verify Z order.
WindowResult applyNativeWindowLayer(QWindow* window,WindowLayer layer);
bool nativeWindowLayerMatches(QWindow* window,WindowLayer layer);
bool nativeWindowAbove(QWindow* upper,QWindow* lower);
}

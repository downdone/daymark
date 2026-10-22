#pragma once

#include "platform/IWindowPlatform.h"

namespace daymark {

// Public Qt APIs first; OS-specific workarounds must stay behind this port.
class QtWindowPlatform final : public IWindowPlatform {
public:
    WindowResult prepare(QWindow* window) override;
    WindowResult setAlwaysOnTop(QWindow* window, bool enabled) override;
    WindowResult resize(QWindow* window, const QSize& size) override;
    WindowResult setVisible(QWindow* window, bool visible) override;
    WindowResult startMove(QWindow* window) override;
    WindowResult setClickThrough(QWindow* window, bool enabled) override;
    WindowResult startResize(QWindow* window) override;
};

} // namespace daymark

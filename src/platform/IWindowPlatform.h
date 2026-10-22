#pragma once

#include <QSize>

class QWindow;

namespace daymark {

enum class WindowError { None, WindowUnavailable, OperationRejected, MoveUnsupported };

struct WindowResult {
    WindowError error = WindowError::None;
    bool succeeded() const { return error == WindowError::None; }
};

class IWindowPlatform {
public:
    virtual ~IWindowPlatform() = default;
    virtual WindowResult prepare(QWindow* window) = 0;
    virtual WindowResult setAlwaysOnTop(QWindow* window, bool enabled) = 0;
    virtual WindowResult resize(QWindow* window, const QSize& size) = 0;
    virtual WindowResult setVisible(QWindow* window, bool visible) = 0;
    virtual WindowResult startMove(QWindow* window) = 0;
    virtual WindowResult setClickThrough(QWindow*, bool) { return {WindowError::OperationRejected}; }
    virtual WindowResult startResize(QWindow*) {return {WindowError::OperationRejected};}
};

} // namespace daymark

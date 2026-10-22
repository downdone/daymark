#include "QtWindowPlatform.h"

#include <QGuiApplication>
#include <QWindow>

namespace daymark {

WindowResult QtWindowPlatform::prepare(QWindow* window)
{
    if (!window)
        return {WindowError::WindowUnavailable};
    // A QML Window nested in Main would otherwise stay above its transient owner,
    // even with the always-on-top hint disabled.
    window->setTransientParent(nullptr);
    window->setFlags(Qt::Tool | Qt::FramelessWindowHint);
    return {};
}

WindowResult QtWindowPlatform::setAlwaysOnTop(QWindow* window, bool enabled)
{
    if (!window)
        return {WindowError::WindowUnavailable};
    const auto oldFlags = window->flags();
    const auto oldGeometry = window->geometry();
    const bool wasVisible = window->isVisible();
    window->setFlag(Qt::WindowStaysOnTopHint, enabled);
    window->setGeometry(oldGeometry);
    window->setVisible(wasVisible);
    if (window->flags().testFlag(Qt::WindowStaysOnTopHint) != enabled) {
        window->setFlags(oldFlags);
        window->setGeometry(oldGeometry);
        window->setVisible(wasVisible);
        return {WindowError::OperationRejected};
    }
    // This checks Qt's accepted flag, not OS-level stacking or focus behavior.
    return {};
}

WindowResult QtWindowPlatform::resize(QWindow* window, const QSize& size)
{
    if (!window)
        return {WindowError::WindowUnavailable};
    if (size.width() <= 0 || size.height() <= 0)
        return {WindowError::OperationRejected};
    const auto oldSize = window->size();
    window->resize(size);
    if (window->size() != size) {
        window->resize(oldSize);
        return {WindowError::OperationRejected};
    }
    return {};
}

WindowResult QtWindowPlatform::setClickThrough(QWindow* window, bool enabled)
{
    if (!window) return {WindowError::WindowUnavailable};
    const auto flags=window->flags(); const auto geometry=window->geometry(); const bool visible=window->isVisible();
    window->setFlag(Qt::WindowTransparentForInput,enabled);
    window->setGeometry(geometry);window->setVisible(visible);
    if(window->flags().testFlag(Qt::WindowTransparentForInput)!=enabled) {
        window->setFlags(flags);window->setGeometry(geometry);window->setVisible(visible);
        return {WindowError::OperationRejected};
    }
    return {};
}

WindowResult QtWindowPlatform::setVisible(QWindow* window, bool visible)
{
    if (!window)
        return {WindowError::WindowUnavailable};
    window->setVisible(visible);
    return window->isVisible() == visible ? WindowResult{} : WindowResult{WindowError::OperationRejected};
}

WindowResult QtWindowPlatform::startMove(QWindow* window)
{
    if (!window || !window->isVisible())
        return {WindowError::WindowUnavailable};
    // Headless probes cannot validate a native drag, even if a plugin accepts it.
    const auto backend = QGuiApplication::platformName();
    if (backend == QStringLiteral("offscreen") || backend == QStringLiteral("minimal"))
        return {WindowError::MoveUnsupported};
    return window->startSystemMove() ? WindowResult{} : WindowResult{WindowError::MoveUnsupported};
}
WindowResult QtWindowPlatform::startResize(QWindow* window)
{
    if(!window || !window->isVisible())return {WindowError::WindowUnavailable};
    const auto backend=QGuiApplication::platformName();
    if(backend==QStringLiteral("offscreen") || backend==QStringLiteral("minimal"))return {WindowError::MoveUnsupported};
    return window->startSystemResize(Qt::RightEdge|Qt::BottomEdge)?WindowResult{}:WindowResult{WindowError::MoveUnsupported};
}

} // namespace daymark

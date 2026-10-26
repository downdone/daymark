#include "QtWindowPlatform.h"
#include "NativeWindowLayer.h"
#include <QEvent>
#include <QPlatformSurfaceEvent>
#include <QPointer>
#include <QTimer>

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

void QtWindowPlatform::watch(QWindow* window)
{
    if (m_watched.contains(window))
        return;
    m_watched.insert(window);
    window->installEventFilter(this);
    connect(window, &QObject::destroyed, this, [this, window] {
        m_watched.remove(window);
        m_pending.remove(window);
    });
}
WindowResult QtWindowPlatform::restoreLayer(QWindow* window)
{
    const auto backend = QGuiApplication::platformName();
    if (backend == QStringLiteral("offscreen") || backend == QStringLiteral("minimal"))
        return {}; // Only Qt state can be tested without a native window system.
    const auto flags = window->flags();
    const auto layer = flags.testFlag(Qt::WindowStaysOnBottomHint) ? WindowLayer::Desktop
        : flags.testFlag(Qt::WindowStaysOnTopHint) ? WindowLayer::Topmost : WindowLayer::Normal;
    return applyNativeWindowLayer(window, layer);
}
bool QtWindowPlatform::eventFilter(QObject* watched, QEvent* event)
{
    auto* window = qobject_cast<QWindow*>(watched);
    const bool created = event->type() == QEvent::PlatformSurface
        && static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated;
    if (window && (created || event->type() == QEvent::Show || event->type() == QEvent::WindowActivate)
        && !m_pending.contains(window)) {
        m_pending.insert(window);
        QPointer<QWindow> guard(window);
        // Let Qt finish changing the handle before restoring its native level.
        QTimer::singleShot(0, this, [this, guard] {
            if (!guard)
                return;
            m_pending.remove(guard.data());
            if (guard->handle() && !restoreLayer(guard.data()).succeeded())
                emit layerRestoreFailed(guard.data());
        });
    }
    return QObject::eventFilter(watched, event);
}
WindowResult QtWindowPlatform::setLayer(QWindow* window, WindowLayer layer)
{
    if (!window)
        return {WindowError::WindowUnavailable};
    watch(window);
    const auto oldFlags = window->flags();
    const auto geometry = window->geometry();
    const bool visible = window->isVisible();
    auto flags = oldFlags;
    flags.setFlag(Qt::WindowStaysOnTopHint, layer == WindowLayer::Topmost);
    flags.setFlag(Qt::WindowStaysOnBottomHint, layer == WindowLayer::Desktop);
    window->setFlags(flags);
    window->setGeometry(geometry);
    window->setVisible(visible);
    const auto result = restoreLayer(window);
    if (!result.succeeded() || window->flags() != flags) {
        window->setFlags(oldFlags);
        window->setGeometry(geometry);
        window->setVisible(visible);
        restoreLayer(window);
        return result.succeeded() ? WindowResult{WindowError::OperationRejected} : result;
    }
    return {};
}
WindowResult QtWindowPlatform::setAlwaysOnTop(QWindow* window, bool enabled)
{
    return setLayer(window, enabled ? WindowLayer::Topmost : WindowLayer::Normal);
}
WindowResult QtWindowPlatform::setDesktopPinned(QWindow* window, bool enabled)
{
    return setLayer(window, enabled ? WindowLayer::Desktop : WindowLayer::Normal);
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
    const auto result=restoreLayer(window);
    if(!result.succeeded()){
        window->setFlags(flags);window->setGeometry(geometry);window->setVisible(visible);restoreLayer(window);
    }
    return result;
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

#include "WindowChrome.h"
#include "diagnostics/RuntimeLog.h"
#include "sync/Diagnostics.h"
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QSettings>

namespace daymark {
WindowChromeController::WindowChromeController(const QString &path,
                                               bool writable, QObject *parent)
    : QObject(parent), m_path(path), m_writable(writable) {
    m_mode = supported() ? 1 : 0;
    QSettings settings(path, QSettings::IniFormat);
    bool ok = false;
    const int stored =
        settings.value(QStringLiteral("window/titleBarMode"), m_mode)
            .toInt(&ok);
    if (supported() && ok && (stored == 0 || stored == 1))
        m_mode = stored;
}
bool WindowChromeController::supported() const {
#ifdef Q_OS_WIN
    return QGuiApplication::platformName() == QStringLiteral("windows");
#else
    return false;
#endif
}
void WindowChromeController::fail(const QString &key, const QString &message,
                                  quint32 nativeCode) {
    m_key = key;
    m_error = message + QStringLiteral(" (%1)").arg(sync::diagnosticCode(key));
    if (nativeCode)
        m_error += tr("，Windows 错误 %1").arg(nativeCode);
    diagnostics::event(
        "window_chrome", "operation.failed",
        {{"error_key", key}, {"native_code", qint64(nativeCode)}}, "warning");
    emit changed();
}
void WindowChromeController::add(WindowChrome *binding) {
    if (m_bindings.contains(binding))
        return;
    m_bindings.append(binding);
    if (!binding->applyMode(m_mode == 1)) {
        const auto nativeCode = binding->nativeError();
        for (auto *item : m_bindings)
            item->applyMode(false);
        m_mode = 0;
        fail("windowChromeNativeFailed",
             tr("标题栏未能应用，已恢复系统原生外观。请重试。"), nativeCode);
    }
}
void WindowChromeController::remove(WindowChrome *binding) {
    m_bindings.removeAll(binding);
}
bool WindowChromeController::requestMode(int mode) {
    if (!supported() || !m_writable || (mode != 0 && mode != 1)) {
        fail("windowChromeUnavailable",
             tr("当前无法切换标题栏，请在 Windows 完成启动后重试。"));
        return false;
    }
    if (mode == m_mode) {
        m_error.clear();
        m_key.clear();
        emit changed();
        return true;
    }
    const int previous = m_mode;
    for (auto *item : m_bindings) {
        if (!item->applyMode(mode == 1)) {
            const auto nativeCode = item->nativeError();
            for (auto *restore : m_bindings)
                restore->applyMode(previous == 1);
            fail("windowChromeNativeFailed",
                 tr("标题栏未能切换，已保留原外观。请重试。"), nativeCode);
            return false;
        }
    }
    QSettings settings(m_path, QSettings::IniFormat);
    const auto old = settings.value(QStringLiteral("window/titleBarMode"));
    settings.setValue(QStringLiteral("window/titleBarMode"), mode);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (old.isValid())
            settings.setValue(QStringLiteral("window/titleBarMode"), old);
        else
            settings.remove(QStringLiteral("window/titleBarMode"));
        settings.sync();
        for (auto *restore : m_bindings)
            restore->applyMode(previous == 1);
        fail("windowChromeSettingsFailed",
             tr("标题栏设置未保存，已保留原外观。请检查数据目录权限后重试。"));
        return false;
    }
    m_mode = mode;
    m_error.clear();
    m_key.clear();
    emit changed();
    return true;
}
WindowChrome::WindowChrome(QObject *parent) : QObject(parent) {}
WindowChrome::~WindowChrome() {
    detachNative();
    if (m_controller && m_registered)
        m_controller->remove(this);
}
void WindowChrome::componentComplete() {
    m_complete = true;
    attach();
}
void WindowChrome::setController(WindowChromeController *controller) {
    if (m_controller == controller)
        return;
    if (m_controller && m_registered)
        m_controller->remove(this);
    m_registered = false;
    m_controller = controller;
    emit targetChanged();
    attach();
}
void WindowChrome::setWindow(QQuickWindow *window) {
    if (m_window == window)
        return;
    detachNative();
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = window;
    m_nativeId = 0;
    if (window)
        window->installEventFilter(this);
    emit targetChanged();
    attach();
}
void WindowChrome::attach() {
    if (!m_complete || !m_window || !m_controller || m_registered)
        return;
    m_registered = true;
    m_controller->add(this);
}
bool WindowChrome::applyMode(bool custom) {
    if (!m_window)
        return true;
    if (custom && (!m_controller || !m_controller->supported()))
        return false;
    if (m_custom == custom)
        return true;
    const auto geometry = nativeFrameGeometry();
    const auto placement = nativePlacement();
    const auto state = m_window->windowState();
    const bool visible = m_window->isVisible();
    QPointer<QQuickItem> focus = m_window->activeFocusItem();
    m_custom = custom;
    // Keep Qt's normal Windows frame contract. FramelessWindowHint makes Qt
    // emulate maximize by geometry alone, bypassing native placement/Snap.
    // The registered Win32 subclass owns non-client drawing and hit testing.
    bool ok = configureNative();
    if (state == Qt::WindowNoState && geometry.isValid())
        ok = restoreNativeFrameGeometry(geometry) && ok;
    else if (!placement.isEmpty())
        ok = restoreNativePlacement(placement, visible) && ok;
    if (m_window->windowState() != state)
        m_window->setWindowState(state);
    if (m_window->isVisible() != visible)
        m_window->setVisible(visible);
    if (focus && m_window->isActive())
        focus->forceActiveFocus(Qt::OtherFocusReason);
    setInteraction(false, false);
    emit customChanged();
    return ok;
}
void WindowChrome::setDark(bool value) {
    if (value == m_dark)
        return;
    m_dark = value;
    configureNative();
    emit appearanceChanged();
}
bool WindowChrome::eventFilter(QObject *object, QEvent *event) {
    if (object == m_window && event->type() == QEvent::PlatformSurface) {
        auto *surface = static_cast<QPlatformSurfaceEvent *>(event);
        if (surface->surfaceEventType() ==
            QPlatformSurfaceEvent::SurfaceCreated)
            configureNative();
        else
            detachNative();
    }
    return QObject::eventFilter(object, event);
}
void WindowChrome::setInteraction(bool hovered, bool pressed) {
    if (m_hovered == hovered && m_pressed == pressed)
        return;
    m_hovered = hovered;
    m_pressed = pressed;
    emit interactionChanged();
}
bool WindowChrome::contains(QQuickItem *item, const QPointF &point) const {
    return item && item->isVisible() && item->isEnabled() &&
           item->contains(item->mapFromScene(point));
}
QVariantList WindowChrome::interactiveItems() const {
    QVariantList result;
    for (const auto &item : m_interactiveItems)
        if (item) result.append(QVariant::fromValue(item.data()));
    return result;
}
void WindowChrome::setInteractiveItems(const QVariantList &items) {
    QList<QPointer<QQuickItem>> next;
    for (const auto &value : items)
        if (auto *item = qvariant_cast<QQuickItem *>(value)) next.append(item);
    if (m_interactiveItems == next) return;
    m_interactiveItems = next;
    emit interactiveItemsChanged();
}
int WindowChrome::hitTest(const QPointF &point) const {
    if (!m_custom || !m_window ||
        m_window->windowState() == Qt::WindowFullScreen)
        return 1;
    if (point.x() < 0 || point.y() < 0 || point.x() >= m_window->width() ||
        point.y() >= m_window->height())
        return 0;
    // Caption buttons retain their full hit height, including the top edge.
    if (contains(m_maximizeButton, point))
        return 9;
    auto inVisibleRegion = [&](QQuickItem *item) { return item && item->isVisible() && item->contains(item->mapFromScene(point)); };
    if (inVisibleRegion(m_buttons))
        return 1;
    for (const auto &item : m_interactiveItems)
        if (inVisibleRegion(item)) return 1;
    return contains(m_titleBar, point) ? 2 : 1;
}
void WindowChrome::toggleMaximize() {
    if (!m_window || !m_window->flags().testFlag(Qt::WindowMaximizeButtonHint))
        return;
    if (m_window->windowState() == Qt::WindowMaximized)
        m_window->showNormal();
    else
        m_window->showMaximized();
}
#ifndef Q_OS_WIN
bool WindowChrome::configureNative() { return true; }
void WindowChrome::detachNative() { m_nativeId = 0; }
QRect WindowChrome::nativeFrameGeometry() const {
    return m_window ? m_window->frameGeometry() : QRect();
}
bool WindowChrome::restoreNativeFrameGeometry(const QRect &) { return true; }
QByteArray WindowChrome::nativePlacement() const { return {}; }
bool WindowChrome::restoreNativePlacement(const QByteArray &, bool) { return true; }
bool WindowChrome::handleNativeMessage(const QByteArray &, void *, qintptr *) {
    return false;
}
void WindowChrome::showSystemMenu() {}
void WindowChrome::popupSystemMenu(const QPoint &) {}
#endif
} // namespace daymark

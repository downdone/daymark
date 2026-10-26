#pragma once

#include "platform/IWindowPlatform.h"
#include <QObject>
#include <QSet>

namespace daymark {

// Owns Qt flags and restoration; native window levels stay behind this port.
class QtWindowPlatform final : public QObject, public IWindowPlatform {
    Q_OBJECT
public:
    WindowResult prepare(QWindow* window) override;
    WindowResult setAlwaysOnTop(QWindow* window, bool enabled) override;
    WindowResult setDesktopPinned(QWindow* window, bool enabled) override;
    WindowResult resize(QWindow* window, const QSize& size) override;
    WindowResult setVisible(QWindow* window, bool visible) override;
    WindowResult startMove(QWindow* window) override;
    WindowResult setClickThrough(QWindow* window, bool enabled) override;
    WindowResult startResize(QWindow* window) override;
signals:
    void layerRestoreFailed(QWindow* window);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    WindowResult setLayer(QWindow* window, WindowLayer layer);
    WindowResult restoreLayer(QWindow* window);
    void watch(QWindow* window);
    QSet<QWindow*> m_watched;
    QSet<QWindow*> m_pending;
};

} // namespace daymark

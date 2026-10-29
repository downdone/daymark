#include "WidgetViewModel.h"
#include "platform/common/QtWindowPlatform.h"
#include <QGuiApplication>
#include <QScreen>
#include <cmath>

namespace daymark {

WidgetViewModel::WidgetViewModel(IWindowPlatform& platform, QObject* parent)
    : QObject(parent), m_platform(platform)
{
    m_saveTimer.setSingleShot(true);m_saveTimer.setInterval(500);
    connect(&m_saveTimer,&QTimer::timeout,this,&WidgetViewModel::saveSettings);
    connect(this,&WidgetViewModel::stateChanged,this,[this]{if(m_settings && m_window)m_saveTimer.start();});
    if(auto* qt=dynamic_cast<QtWindowPlatform*>(&m_platform))connect(qt,&QtWindowPlatform::layerRestoreFailed,this,[this](QWindow* window){if(window==m_window)accept({WindowError::OperationRejected});});
}

bool WidgetViewModel::attachWindow(QWindow* window)
{
    if (m_window)
        disconnect(m_window, nullptr, this, nullptr);
    m_window = nullptr;
    m_mini = false;
    m_miniCollapsed = false;
    if (!accept(m_platform.prepare(window)) || !accept(m_platform.setAlwaysOnTop(window, true))) {
        emit stateChanged();
        return false;
    }
    m_window = window;
    m_floatingSize = window->size();
    connect(window, &QWindow::visibleChanged, this, &WidgetViewModel::stateChanged);
    connect(window, &QWindow::flagsChanged, this, &WidgetViewModel::stateChanged);
    auto moved=[this]{if(m_settings)m_saveTimer.start();};
    connect(window,&QWindow::xChanged,this,moved);connect(window,&QWindow::yChanged,this,moved);
    connect(window,&QWindow::widthChanged,this,moved);connect(window,&QWindow::heightChanged,this,moved);
    connect(window, &QObject::destroyed, this, [this] {
        m_window = nullptr;
        emit stateChanged();
    });
    connect(qGuiApp,&QGuiApplication::screenRemoved,this,[this]{normalizeGeometry();});
    connect(qGuiApp,&QGuiApplication::screenAdded,this,[this](QScreen* screen){connect(screen,&QScreen::availableGeometryChanged,this,[this]{normalizeGeometry();});normalizeGeometry();});
    for(auto* screen:QGuiApplication::screens())connect(screen,&QScreen::availableGeometryChanged,this,[this]{normalizeGeometry();});
    if(m_settings) {
        m_miniAutoCollapse=m_settings->value(QStringLiteral("widget/miniAutoCollapse"),false).toBool();
        m_locked=m_settings->value(QStringLiteral("widget/locked"),false).toBool();
        setBackgroundOpacity(m_settings->value(QStringLiteral("widget/backgroundOpacity"),0.9).toDouble());
        m_closeToTray=m_settings->value(QStringLiteral("main/closeToTray"),true).toBool();
        const auto size=m_settings->value(QStringLiteral("widget/floatingSize"),QSize(340,360)).toSize();
        m_platform.resize(window,size.expandedTo(QSize(260,190)));
        m_floatingSize=window->size();
        QPoint position=m_settings->value(QStringLiteral("widget/position"),QPoint(80,80)).toPoint();
        const auto savedScreen=m_settings->value(QStringLiteral("widget/screenName")).toString();
        if(!savedScreen.isEmpty()) {
            QScreen* matching=nullptr;for(auto* screen:QGuiApplication::screens())if(screen->name()==savedScreen){matching=screen;break;}
            if(matching) {
                const auto available=matching->availableGeometry();
                if(m_settings->value(QStringLiteral("widget/availableGeometry"),available).toRect()!=available) {
                    const auto normalized=m_settings->value(QStringLiteral("widget/normalizedPosition"),QPointF(0.05,0.05)).toPointF();
                    position=available.topLeft()+QPoint(qRound(normalized.x()*available.width()),qRound(normalized.y()*available.height()));
                }
            } else if(auto* primary=QGuiApplication::primaryScreen())position=primary->availableGeometry().topLeft()+QPoint(24,24);
        }
        window->setPosition(position);
        setAlwaysOnTop(m_settings->value(QStringLiteral("widget/alwaysOnTop"),true).toBool());
        if(m_settings->value(QStringLiteral("widget/desktopPinned"),false).toBool())setDesktopPinned(true);
        setMini(m_settings->value(QStringLiteral("widget/mini"),false).toBool());
        normalizeGeometry();
    }
    emit stateChanged();
    return true;
}

bool WidgetViewModel::setMini(bool mini)
{
    if (!m_window)
        return accept({WindowError::WindowUnavailable});
    if (mini == m_mini)
        return accept({});
    const auto floatingSize = mini ? m_window->size() : m_floatingSize;
    const int width=m_miniCollapsed ? m_miniSize.width() : m_window->width();
    if (!accept(m_platform.resize(m_window,QSize{width,mini ? 48 : floatingSize.height()})))
        return false;
    if (mini)
        m_floatingSize = floatingSize;
    m_mini = mini;
    m_miniCollapsed = false;
    emit stateChanged();
    return true;
}

bool WidgetViewModel::setMiniAutoCollapse(bool enabled)
{
    if(!enabled && m_miniCollapsed && !setMiniCollapsed(false))return false;
    m_miniAutoCollapse=enabled;
    emit stateChanged();
    return accept({});
}

bool WidgetViewModel::setMiniCollapsed(bool collapsed,int diameter)
{
    if(!m_window)return accept({WindowError::WindowUnavailable});
    if(collapsed && (!m_mini || !m_miniAutoCollapse || clickThrough()))return accept({WindowError::OperationRejected});
    if(collapsed==m_miniCollapsed)return accept({});
    const auto miniSize=collapsed ? m_window->size() : m_miniSize;
    const int side=qBound(36,diameter,96);
    if(!accept(m_platform.resize(m_window,collapsed ? QSize(side,side) : miniSize)))return false;
    if(collapsed)m_miniSize=miniSize;
    m_miniCollapsed=collapsed;
    normalizeGeometry();
    emit stateChanged();
    return true;
}

bool WidgetViewModel::setAlwaysOnTop(bool enabled)
{
    const bool succeeded = accept(m_platform.setAlwaysOnTop(m_window, enabled));
    // Re-evaluate checkable UI controls even when the platform rejects a toggle.
    emit stateChanged();
    return succeeded;
}
bool WidgetViewModel::setDesktopPinned(bool enabled)
{
    const bool success=accept(m_platform.setDesktopPinned(m_window,enabled));emit stateChanged();return success;
}

bool WidgetViewModel::setVisible(bool visible)
{
    if (!accept(m_platform.setVisible(m_window, visible)))
        return false;
    emit stateChanged();
    return true;
}

bool WidgetViewModel::startMove()
{
    if(m_locked)return accept({WindowError::OperationRejected});
    return accept(m_platform.startMove(m_window));
}
bool WidgetViewModel::startResize()
{
    if(m_locked || m_mini)return accept({WindowError::OperationRejected});
    return accept(m_platform.startResize(m_window));
}

void WidgetViewModel::configureSettings(const QString& path)
{m_settings=std::make_unique<QSettings>(path,QSettings::IniFormat);}

void WidgetViewModel::setTrayAvailable(bool available)
{
    if(!available && clickThrough() && !setClickThrough(false)) {
        // If restoring input fails, hide the passive window; the main app stays reachable.
        setVisible(false);
    }
    m_trayAvailable=available;emit stateChanged();
}
bool WidgetViewModel::setClickThrough(bool enabled)
{
    if(enabled && !m_trayAvailable)return accept({WindowError::OperationRejected});
    if(enabled && m_miniCollapsed && !setMiniCollapsed(false))return false;
    const bool success=accept(m_platform.setClickThrough(m_window,enabled));emit stateChanged();return success;
}
void WidgetViewModel::setLocked(bool locked){m_locked=locked;emit stateChanged();}
void WidgetViewModel::setBackgroundOpacity(double opacity)
{m_backgroundOpacity=std::isfinite(opacity)?qBound(0.6,opacity,1.0):0.9;emit stateChanged();}
void WidgetViewModel::setCloseToTray(bool enabled){m_closeToTray=enabled;emit stateChanged();}
void WidgetViewModel::normalizeGeometry()
{
    if(!m_window)return;
    auto* target=m_window->screen();
    bool intersects=false;
    for(auto* screen:QGuiApplication::screens())if(screen->availableGeometry().intersects(m_window->geometry())){target=screen;intersects=true;break;}
    if(!target || !QGuiApplication::screens().contains(target))target=QGuiApplication::primaryScreen();
    if(!target)return;
    const auto available=target->availableGeometry();
    const auto size=m_window->size().boundedTo(available.size());
    QPoint position=intersects?m_window->position():available.topLeft()+QPoint(24,24);
    position.setX(qBound(available.left(),position.x(),available.right()-size.width()+1));
    position.setY(qBound(available.top(),position.y(),available.bottom()-size.height()+1));
    m_window->setGeometry(QRect(position,size));
}
void WidgetViewModel::resetPosition()
{
    if(!m_window)return;
    if(auto* screen=QGuiApplication::primaryScreen())m_window->setPosition(screen->availableGeometry().topLeft()+QPoint(24,24));
    normalizeGeometry();
}
void WidgetViewModel::saveSettings()
{
    if(!m_settings || !m_window)return;
    m_settings->setValue(QStringLiteral("widget/floatingSize"),QSize(m_miniCollapsed ? m_miniSize.width() : m_window->width(),m_mini?m_floatingSize.height():m_window->height()));
    m_settings->setValue(QStringLiteral("widget/position"),m_window->position());
    m_settings->setValue(QStringLiteral("widget/mini"),m_mini);
    m_settings->setValue(QStringLiteral("widget/miniAutoCollapse"),m_miniAutoCollapse);
    m_settings->setValue(QStringLiteral("widget/visible"),visible());
    m_settings->setValue(QStringLiteral("widget/alwaysOnTop"),alwaysOnTop());
    m_settings->setValue(QStringLiteral("widget/desktopPinned"),desktopPinned());
    m_settings->setValue(QStringLiteral("widget/locked"),m_locked);
    m_settings->setValue(QStringLiteral("widget/backgroundOpacity"),m_backgroundOpacity);
    m_settings->setValue(QStringLiteral("main/closeToTray"),m_closeToTray);
    if(auto* screen=m_window->screen()) {
        m_settings->setValue(QStringLiteral("widget/screenName"),screen->name());
        m_settings->setValue(QStringLiteral("widget/dpr"),screen->devicePixelRatio());
        const auto rect=screen->availableGeometry();
        m_settings->setValue(QStringLiteral("widget/availableGeometry"),rect);
        m_settings->setValue(QStringLiteral("widget/normalizedPosition"),QPointF(double(m_window->x()-rect.x())/qMax(1,rect.width()),double(m_window->y()-rect.y())/qMax(1,rect.height())));
    }
    m_settings->sync();
    if(m_settings->status()!=QSettings::NoError)accept({WindowError::OperationRejected});
}

bool WidgetViewModel::accept(WindowResult result)
{
    if (m_error != result.error) {
        m_error = result.error;
        emit errorChanged();
    }
    return result.succeeded();
}

void WidgetViewModel::clearError()
{
    accept({});
}

QString WidgetViewModel::errorText() const
{
    switch (m_error) {
    case WindowError::None: return {};
    case WindowError::WindowUnavailable:
        return tr("组件窗口不可用。请先显示组件；若仍失败，请重新启动 Daymark。");
    case WindowError::OperationRejected:
        return tr("窗口设置未能应用，已保留原设置。请重试该操作。");
    case WindowError::MoveUnsupported:
        return tr("未能开始系统拖动。请按住组件表头拖动重试；当前窗口后端也可能不支持此操作。");
    }
    return {};
}

} // namespace daymark

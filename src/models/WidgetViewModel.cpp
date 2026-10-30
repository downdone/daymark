#include "WidgetViewModel.h"
#include "platform/common/QtWindowPlatform.h"
#include <QGuiApplication>
#include <QScreen>
#include <QRegion>
#include <QPainterPath>
#include <cmath>

namespace daymark {
WidgetViewModel::WidgetViewModel(IWindowPlatform& platform, QObject* parent)
    : QObject(parent), m_platform(platform)
{
    m_saveTimer.setSingleShot(true);m_saveTimer.setInterval(500);
    connect(&m_saveTimer,&QTimer::timeout,this,&WidgetViewModel::saveSettings);
    connect(this,&WidgetViewModel::stateChanged,this,[this]{if(m_settings && m_window && !m_geometryChanging)m_saveTimer.start();});
    m_hoverTimer.setSingleShot(true);m_hoverTimer.setInterval(150);
    m_idleTimer.setSingleShot(true);m_idleTimer.setInterval(600);
    m_returnTimer.setSingleShot(true);m_returnTimer.setInterval(1000);
    connect(&m_returnTimer,&QTimer::timeout,this,[this]{
        if(!visible() || clickThrough() || !m_presentation.temporaryOpen || m_hovered || m_pressed || m_menu || m_keyboard || m_moving)return;
        auto candidate=m_presentation;
        if(candidate.returnToSelected())applyPresentation(candidate);
    });
    connect(&m_hoverTimer,&QTimer::timeout,this,[this]{if(m_hovered && !m_pressed && !m_menu && !m_keyboard && !m_moving)previewForm(WidgetPresentation::Mini);});
    connect(&m_idleTimer,&QTimer::timeout,this,[this]{if(!m_hovered && !m_pressed && !m_menu && !m_keyboard && !m_moving)previewForm(WidgetPresentation::Ball);});
    m_moveTimer.setInterval(100);
    connect(&m_moveTimer,&QTimer::timeout,this,[this]{if(QGuiApplication::mouseButtons()==Qt::NoButton)finishMove();});
    if(auto* qt=dynamic_cast<QtWindowPlatform*>(&m_platform))connect(qt,&QtWindowPlatform::layerRestoreFailed,this,[this](QWindow* window){if(window==m_window)accept({WindowError::OperationRejected});});
}

QList<WidgetScreen> WidgetViewModel::screens() const
{
    QList<WidgetScreen> result;
    const auto primary=QGuiApplication::primaryScreen();
    if(primary)result.append({primary->name(),primary->availableGeometry()});
    for(auto* screen:QGuiApplication::screens())if(screen!=primary)result.append({screen->name(),screen->availableGeometry()});
    return result;
}
QSize WidgetViewModel::formSize(int form) const
{
    if(form==WidgetPresentation::Ball)return {m_ballDiameter,m_ballDiameter};
    if(form==WidgetPresentation::Mini)return {m_miniSize.width(),m_miniHeight};
    return m_floatingSize;
}
void WidgetViewModel::captureUserGeometry()
{
    if(!m_window || m_geometryChanging || m_pendingCollapse)return;
    const auto rect=m_window->geometry();const auto available=screens();
    int index=WidgetPlacementPolicy::screenFor(rect,available);
    if(index<0)index=available.isEmpty()?-1:0;
    if(index<0)return;
    m_anchor=WidgetPlacementPolicy::capture(rect,available[index]);m_anchorValid=true;
    if(m_renderForm==WidgetPresentation::Panel)m_floatingSize=rect.size();
    m_appliedGeometry=rect;
}
void WidgetViewModel::watchGeometry()
{
    if(m_geometryChanging)return;
    if(m_menu){m_menu=false;emit stateChanged();}
    if(m_settings)m_saveTimer.start();
}
bool WidgetViewModel::attachWindow(QWindow* window)
{
    if(m_window)disconnect(m_window,nullptr,this,nullptr);
    m_anchorValid=false;
    m_window=nullptr;m_renderForm=WidgetPresentation::Panel;m_pendingCollapse=false;m_panelMotion=false;
    if(!accept(m_platform.prepare(window)) || !accept(m_platform.setAlwaysOnTop(window,true))){emit stateChanged();return false;}
    m_window=window;m_floatingSize=window->size();m_presentation={};
    connect(window,&QWindow::visibleChanged,this,[this]{scheduleAutomatic();emit stateChanged();});
    connect(window,&QWindow::flagsChanged,this,&WidgetViewModel::stateChanged);
    connect(window,&QWindow::xChanged,this,&WidgetViewModel::watchGeometry);connect(window,&QWindow::yChanged,this,&WidgetViewModel::watchGeometry);
    connect(window,&QWindow::widthChanged,this,&WidgetViewModel::watchGeometry);connect(window,&QWindow::heightChanged,this,&WidgetViewModel::watchGeometry);
    connect(window,&QObject::destroyed,this,[this]{m_window=nullptr;m_hoverTimer.stop();m_idleTimer.stop();m_returnTimer.stop();m_moveTimer.stop();emit stateChanged();});
    connect(qGuiApp,&QGuiApplication::screenRemoved,this,[this]{normalizeGeometry();});
    connect(qGuiApp,&QGuiApplication::screenAdded,this,[this](QScreen* screen){connect(screen,&QScreen::availableGeometryChanged,this,[this]{normalizeGeometry();});normalizeGeometry();});
    for(auto* screen:QGuiApplication::screens())connect(screen,&QScreen::availableGeometryChanged,this,[this]{normalizeGeometry();});
    if(m_settings){
        m_autoCompact=m_settings->value("widget/miniAutoCollapse",false).toBool();
        m_locked=m_settings->value("widget/locked",false).toBool();
        m_backgroundOpacity=qBound(0.6,m_settings->value("widget/backgroundOpacity",0.9).toDouble(),1.0);
        m_closeToTray=m_settings->value("main/closeToTray",true).toBool();
        m_requestedVisible=m_settings->value("widget/visible",true).toBool();
        m_floatingSize=m_settings->value("widget/floatingSize",QSize(340,360)).toSize().expandedTo(QSize(260,190));
        m_geometryChanging=true;m_platform.resize(window,m_floatingSize);
        window->setPosition(m_settings->value("widget/position",QPoint(80,80)).toPoint());m_geometryChanging=false;
        if(m_settings->contains("widget/v2/enabledForms")){
            m_autoCompact=m_settings->value("widget/v2/autoCompact",false).toBool();
            m_requestedVisible=m_settings->value("widget/v2/visible",true).toBool();
            m_floatingSize=m_settings->value("widget/v2/panelSize",QSize(340,360)).toSize().expandedTo(QSize(260,190));
            m_miniSize.setWidth(qBound(260,m_settings->value("widget/v2/miniWidth",340).toInt(),1200));
            m_presentation.setEnabled(m_settings->value("widget/v2/enabledForms",3).toInt());
            m_presentation.select(m_settings->value("widget/v2/selectedForm",0).toInt());
            m_presentation.history={m_presentation.selected};
            for(const auto& value:m_settings->value("widget/v2/history").toList()){
                const int form=value.toInt();if(form>=0 && form<=2 && !m_presentation.history.contains(WidgetPresentation::Form(form)))m_presentation.history.append(WidgetPresentation::Form(form));
            }
            m_anchor.screen=m_settings->value("widget/v2/screen").toString();
            m_anchor.reference=m_settings->value("widget/v2/available").toRect();
            m_anchor.fraction=m_settings->value("widget/v2/anchor",QPointF(0.05,0.05)).toPointF();
            m_anchor.right=m_settings->value("widget/v2/right",false).toBool();m_anchor.bottom=m_settings->value("widget/v2/bottom",false).toBool();
            m_anchorValid=m_settings->contains("widget/v2/anchor") && std::isfinite(m_anchor.fraction.x()) && std::isfinite(m_anchor.fraction.y());
        }else{
            if(m_autoCompact)m_presentation.enabled|=1<<WidgetPresentation::Ball;
            m_presentation.select(m_settings->value("widget/mini",false).toBool()?1:0);
            // Migration keeps the old compact width, but subsequent panel resizes do not change it.
            m_miniSize.setWidth(m_floatingSize.width());
        }
        setAlwaysOnTop(m_settings->value("widget/alwaysOnTop",true).toBool());
        if(m_settings->value("widget/desktopPinned",false).toBool())setDesktopPinned(true);
    }
    if(!m_anchorValid)captureUserGeometry();
    m_appliedGeometry=m_window->geometry();
    const auto candidate=m_presentation;
    m_presentation.effective=WidgetPresentation::Panel;
    if(!applyPresentation(candidate,false))return false;
    emit stateChanged();return true;
}

bool WidgetViewModel::applyPresentation(const WidgetPresentation& candidate,bool animate)
{
    if(!m_window)return accept({WindowError::WindowUnavailable});
    if(m_geometryChanging)return false;
    const auto previous=m_presentation;const auto oldRender=m_renderForm;const auto original=m_window->geometry();
    const bool previousMotion=m_panelMotion;
    const bool previousPending=m_pendingCollapse;
    if(!m_pendingCollapse && (original.topLeft()!=m_appliedGeometry.topLeft() || (oldRender==WidgetPresentation::Panel && original.size()!=m_appliedGeometry.size())))captureUserGeometry();
    m_hoverTimer.stop();m_idleTimer.stop();m_returnTimer.stop();
    if(!candidate.enabled){
        if(!accept(m_platform.setVisible(m_window,false)))return false;
        m_presentation=candidate;m_pendingCollapse=false;m_panelMotion=false;++m_transitionId;if(QGuiApplication::platformName()!=QStringLiteral("offscreen") && QGuiApplication::platformName()!=QStringLiteral("minimal"))m_window->setMask(QRegion());emit stateChanged();return true;
    }
    m_presentation=candidate;m_pendingCollapse=false;m_panelMotion=false;++m_transitionId;
    m_revealFrom=oldRender==WidgetPresentation::Ball?0:1;
    const bool visual=animate && m_visualTransitions && visible() && !clickThrough();
    if(visual && (oldRender==WidgetPresentation::Mini || oldRender==WidgetPresentation::Panel) && candidate.effective==WidgetPresentation::Ball){
        m_panelMotion=oldRender==WidgetPresentation::Panel;
        m_collapseRollback=previous;m_pendingCollapse=true;emit stateChanged();scheduleAutomatic();return accept({});
    }
    m_panelMotion=visual && candidate.effective==WidgetPresentation::Panel &&
        (oldRender==WidgetPresentation::Ball || previousMotion ||
         (oldRender==WidgetPresentation::Mini && candidate.temporaryOpen && candidate.selected==WidgetPresentation::Ball &&
          formSize(WidgetPresentation::Panel).width()>=original.width() && formSize(WidgetPresentation::Panel).height()>=original.height()));
    m_geometryChanging=true;m_renderForm=candidate.effective;emit stateChanged();
    const auto target=WidgetPlacementPolicy::resolve(m_anchor,formSize(m_renderForm),screens());
    const auto result=m_platform.resize(m_window,target.size());
    if(result.succeeded()){
        m_window->setPosition(target.topLeft());m_appliedGeometry=m_window->geometry();if(QGuiApplication::platformName()!=QStringLiteral("offscreen") && QGuiApplication::platformName()!=QStringLiteral("minimal"))m_window->setMask(QRegion());m_window->requestUpdate();
    }else{
        m_presentation=previous;m_renderForm=oldRender;m_panelMotion=previousMotion;m_pendingCollapse=previousPending;emit stateChanged();
        m_window->setGeometry(original);m_appliedGeometry=original;
    }
    m_geometryChanging=false;accept(result);emit stateChanged();scheduleAutomatic();return result.succeeded();
}
bool WidgetViewModel::selectForm(int form)
{
    auto candidate=m_presentation;
    if(!candidate.select(form))return accept({WindowError::OperationRejected});
    return applyPresentation(candidate);
}
bool WidgetViewModel::setFormEnabled(int form,bool enabled)
{
    if(form<0 || form>2)return accept({WindowError::OperationRejected});
    auto candidate=m_presentation;candidate.setEnabled(enabled?candidate.enabled|(1<<form):candidate.enabled&~(1<<form));
    if(!applyPresentation(candidate,false))return false;
    if(enabled && m_requestedVisible && !visible())return setVisible(true);
    return true;
}
void WidgetViewModel::activatePrimary()
{
    auto candidate=m_presentation;
    if(candidate.openPrimary())applyPresentation(candidate);
    else emit openMainRequested();
}
void WidgetViewModel::returnToCompact(){if(canCompact())selectForm(m_presentation.fallback(true));}
bool WidgetViewModel::setMini(bool mini){return selectForm(mini?WidgetPresentation::Mini:WidgetPresentation::Panel);}
bool WidgetViewModel::setAutoCompact(bool enabled)
{
    if(enabled && (!m_presentation.allows(1) || !m_presentation.allows(2)))return accept({WindowError::OperationRejected});
    return setMiniAutoCollapse(enabled);
}
bool WidgetViewModel::setMiniAutoCollapse(bool enabled)
{
    const bool old=m_autoCompact;
    // Compatibility command also enables Ball when migrating the old single checkbox.
    auto candidate=m_presentation;
    if(enabled)candidate.setEnabled(candidate.enabled|(1<<WidgetPresentation::Ball));
    if(!enabled && !candidate.temporaryOpen)candidate.effective=candidate.selected;
    m_autoCompact=enabled;
    if(!applyPresentation(candidate,false)){m_autoCompact=old;return false;}
    scheduleAutomatic();return true;
}
bool WidgetViewModel::setMiniLayout(int height,int diameter)
{
    const int oldHeight=m_miniHeight,oldDiameter=m_ballDiameter;
    m_miniHeight=qBound(48,height,512);m_ballDiameter=qBound(36,diameter,96);
    if(oldHeight==m_miniHeight && oldDiameter==m_ballDiameter)return true;
    if(mini() && !applyPresentation(m_presentation,false)){m_miniHeight=oldHeight;m_ballDiameter=oldDiameter;return false;}
    return true;
}
bool WidgetViewModel::setMiniCollapsed(bool collapsed,int diameter)
{
    if(!mini() || !m_autoCompact || !m_presentation.allows(WidgetPresentation::Ball) || clickThrough())return accept({WindowError::OperationRejected});
    auto candidate=m_presentation;candidate.preview(collapsed?2:1);
    const auto old=m_ballDiameter;m_ballDiameter=qBound(36,diameter,96);
    if(applyPresentation(candidate,false))return true;
    m_ballDiameter=old;return false;
}
void WidgetViewModel::previewForm(int form)
{
    if(!m_autoCompact || !visible() || clickThrough() || !m_presentation.allows(1) || !m_presentation.allows(2))return;
    auto candidate=m_presentation;
    if(candidate.effective!=form && candidate.preview(form))applyPresentation(candidate);
}
void WidgetViewModel::scheduleAutomatic()
{
    const bool held=m_pressed || m_menu || m_keyboard || m_moving;
    if(!m_window || !visible() || !m_presentation.enabled || clickThrough() || held){m_hoverTimer.stop();m_idleTimer.stop();m_returnTimer.stop();return;}
    // A click borrows the larger surface; it never changes the user's docking form.
    if(m_presentation.temporaryOpen){
        m_hoverTimer.stop();m_idleTimer.stop();
        if(m_hovered)m_returnTimer.stop();
        else if(!m_returnTimer.isActive())m_returnTimer.start();
        return;
    }
    m_returnTimer.stop();
    if(!m_autoCompact || m_presentation.selected==0 || !m_presentation.allows(1) || !m_presentation.allows(2)){m_hoverTimer.stop();m_idleTimer.stop();return;}
    if(m_hovered){m_idleTimer.stop();if(m_presentation.effective==2 && !m_hoverTimer.isActive())m_hoverTimer.start();}
    else{m_hoverTimer.stop();if(m_presentation.effective==1 && !m_idleTimer.isActive())m_idleTimer.start();}
}
void WidgetViewModel::setInteraction(bool hovered,bool pressed,bool menu,bool keyboard)
{m_hovered=hovered;m_pressed=pressed;m_menu=menu;m_keyboard=keyboard;scheduleAutomatic();}
void WidgetViewModel::enableVisualTransitions(){m_visualTransitions=true;}
void WidgetViewModel::finishTransition(int id)
{
    if(id!=m_transitionId)return;
    if(!m_pendingCollapse){if(m_panelMotion){m_panelMotion=false;emit stateChanged();}return;}
    const auto candidate=m_presentation;const auto rollback=m_collapseRollback;m_pendingCollapse=false;
    if(!applyPresentation(candidate,false)){
        m_presentation=rollback;m_panelMotion=false;m_revealFrom=1;++m_transitionId;emit stateChanged();scheduleAutomatic();
    }
}
void WidgetViewModel::setRevealRegion(const QRect& rectangle,double radius)
{
    if(!m_window || m_geometryChanging)return;
    // Headless backends have no OS input region; never claim this as native proof.
    if(QGuiApplication::platformName()==QStringLiteral("offscreen") || QGuiApplication::platformName()==QStringLiteral("minimal"))return;
    const auto rect=rectangle.intersected(QRect(QPoint(),m_window->size()));
    if(rect.isEmpty())return;
    QPainterPath path;radius=qBound(0.,radius,qMin(rect.width(),rect.height())/2.);
    path.addRoundedRect(QRectF(rect),radius,radius);
    const QRegion region(path.toFillPolygon().toPolygon());
    if(m_window->mask()==region)return;
    m_window->setMask(region);
}
bool WidgetViewModel::setAlwaysOnTop(bool enabled){const bool ok=accept(m_platform.setAlwaysOnTop(m_window,enabled));emit stateChanged();return ok;}
bool WidgetViewModel::setDesktopPinned(bool enabled){const bool ok=accept(m_platform.setDesktopPinned(m_window,enabled));emit stateChanged();return ok;}
bool WidgetViewModel::setLayer(int value){if(value<0 || value>2)return accept({WindowError::OperationRejected});return value==2?setDesktopPinned(true):setAlwaysOnTop(value==1);}
bool WidgetViewModel::setVisible(bool value)
{
    if(value && !m_presentation.enabled){emit configurationRequested();accept({WindowError::OperationRejected});emit stateChanged();return false;}
    if(value){auto candidate=m_presentation;candidate.returnToSelected();if(!applyPresentation(candidate,false))return false;}
    if(!accept(m_platform.setVisible(m_window,value))){emit stateChanged();return false;}
    m_requestedVisible=value;if(!value){m_hoverTimer.stop();m_idleTimer.stop();m_returnTimer.stop();m_pendingCollapse=false;m_panelMotion=false;++m_transitionId;}
    emit stateChanged();scheduleAutomatic();return true;
}
bool WidgetViewModel::startMove()
{
    if(m_locked)return accept({WindowError::OperationRejected});
    const auto result=m_platform.startMove(m_window);
    if(result.succeeded()){m_moving=true;m_hoverTimer.stop();m_idleTimer.stop();m_returnTimer.stop();m_moveTimer.start();}
    return accept(result);
}
bool WidgetViewModel::startResize()
{
    if(m_locked || mini())return accept({WindowError::OperationRejected});
    const auto result=m_platform.startResize(m_window);
    if(result.succeeded()){m_moving=true;m_moveTimer.start();}
    return accept(result);
}
void WidgetViewModel::finishMove(){m_moveTimer.stop();if(m_moving){m_moving=false;captureUserGeometry();normalizeGeometry();emit stateChanged();scheduleAutomatic();}}
void WidgetViewModel::configureSettings(const QString& path){m_settings=std::make_unique<QSettings>(path,QSettings::IniFormat);}
void WidgetViewModel::setTrayAvailable(bool available){if(!available && clickThrough() && !setClickThrough(false))setVisible(false);m_trayAvailable=available;emit stateChanged();}
bool WidgetViewModel::setClickThrough(bool enabled)
{
    if(enabled && !m_trayAvailable)return accept({WindowError::OperationRejected});
    if(enabled && (m_pendingCollapse || m_panelMotion) && !applyPresentation(m_presentation,false))return false;
    const bool ok=accept(m_platform.setClickThrough(m_window,enabled));emit stateChanged();scheduleAutomatic();return ok;
}
void WidgetViewModel::setLocked(bool locked){m_locked=locked;emit stateChanged();}
void WidgetViewModel::setBackgroundOpacity(double opacity){m_backgroundOpacity=std::isfinite(opacity)?qBound(0.6,opacity,1.0):0.9;emit stateChanged();}
void WidgetViewModel::setCloseToTray(bool enabled){m_closeToTray=enabled;emit stateChanged();}
void WidgetViewModel::normalizeGeometry()
{
    if(!m_window || m_geometryChanging)return;
    m_geometryChanging=true;
    const auto rect=WidgetPlacementPolicy::resolve(m_anchor,formSize(m_renderForm),screens());
    m_window->setGeometry(rect);m_appliedGeometry=m_window->geometry();m_geometryChanging=false;emit stateChanged();
}
void WidgetViewModel::resetPosition()
{
    if(!m_window)return;
    const auto available=screens();if(available.isEmpty())return;
    m_anchor=WidgetPlacementPolicy::capture({available.first().available.topLeft()+QPoint(24,24),formSize(m_renderForm)},available.first());m_anchorValid=true;
    normalizeGeometry();
}
void WidgetViewModel::saveSettings()
{
    if(!m_settings || !m_window || m_geometryChanging)return;
    if(!m_pendingCollapse && !m_moving && m_window->geometry()!=m_appliedGeometry)captureUserGeometry();
    QVariantList history;for(const auto form:m_presentation.history)history.append(int(form));m_settings->setValue("widget/v2/history",history);
    m_settings->setValue("widget/v2/enabledForms",m_presentation.enabled);m_settings->setValue("widget/v2/selectedForm",int(m_presentation.selected));
    m_settings->setValue("widget/v2/autoCompact",m_autoCompact);m_settings->setValue("widget/v2/visible",m_requestedVisible);
    m_settings->setValue("widget/v2/panelSize",m_floatingSize);m_settings->setValue("widget/v2/miniWidth",m_miniSize.width());
    m_settings->setValue("widget/v2/screen",m_anchor.screen);m_settings->setValue("widget/v2/available",m_anchor.reference);
    m_settings->setValue("widget/v2/anchor",m_anchor.fraction);m_settings->setValue("widget/v2/right",m_anchor.right);m_settings->setValue("widget/v2/bottom",m_anchor.bottom);
    m_settings->setValue("widget/alwaysOnTop",alwaysOnTop());m_settings->setValue("widget/desktopPinned",desktopPinned());
    m_settings->setValue("widget/locked",m_locked);m_settings->setValue("widget/backgroundOpacity",m_backgroundOpacity);m_settings->setValue("main/closeToTray",m_closeToTray);
    m_settings->sync();if(m_settings->status()!=QSettings::NoError)accept({WindowError::OperationRejected});
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

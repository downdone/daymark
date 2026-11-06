#include "WidgetViewModel.h"
#include "platform/common/QtWindowPlatform.h"
#include <QGuiApplication>
#include <QScreen>
#include <QRegion>
#include <QPainterPath>
#include <cmath>
#include <atomic>
#include <QQuickWindow>
#include <QCursor>

namespace daymark {
struct WidgetFrameFence {std::atomic<int> pending{-1},synced{-1};};
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
    m_geometryTimer.setSingleShot(true);m_geometryTimer.setInterval(0);
    connect(&m_geometryTimer,&QTimer::timeout,this,[this]{if(m_window && !m_geometryChanging && !m_moving && m_window->geometry()!=m_appliedGeometry){captureUserGeometry();applyPresentation(m_presentation,false);}});
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
    return m_floatingSize.expandedTo(m_panelMinimum);
}
void WidgetViewModel::captureUserGeometry()
{
    if(!m_window || m_geometryChanging || m_pendingCollapse)return;
    const auto actual=m_window->geometry();
    if(m_anchorValid && !m_moving){
        bool sameTopology=false;
        for(const auto& screen:screens())if(screen.id==m_anchor.screen && screen.available==m_anchor.reference){sameTopology=true;break;}
        if(!sameTopology)return; // OS recovery must not overwrite a missing screen's saved intent.
    }
    auto rect=actual;
    if(m_visualTransitions && !m_layout.canvas.isEmpty()){
        rect=m_layout.forms[m_renderForm].translated(actual.topLeft());
        if(m_renderForm==WidgetPresentation::Panel && actual.size()!=m_appliedGeometry.size())rect.setSize(rect.size()+actual.size()-m_appliedGeometry.size());
    }
    const auto available=screens();int index=WidgetPlacementPolicy::screenFor(rect,available);
    if(index<0)index=available.isEmpty()?-1:0;if(index<0)return;
    m_anchor=WidgetPlacementPolicy::capture(rect,available[index]);m_anchorValid=true;
    if(m_renderForm==WidgetPresentation::Panel)m_floatingSize=rect.size();
    m_appliedGeometry=actual;
}
void WidgetViewModel::watchGeometry()
{
    if(m_geometryChanging || !m_window || m_window->geometry()==m_appliedGeometry)return;
    if(m_settings)m_saveTimer.start();
    if(m_visualTransitions && !m_moving)m_geometryTimer.start();
}
bool WidgetViewModel::attachWindow(QWindow* window)
{
    if(m_window)disconnect(m_window,nullptr,this,nullptr);
    m_anchorValid=false;
    m_window=nullptr;m_renderForm=WidgetPresentation::Panel;m_pendingCollapse=false;m_panelMotion=false;m_visualPending=false;
    if(!accept(m_platform.prepare(window)) || !accept(m_platform.setAlwaysOnTop(window,true))){emit stateChanged();return false;}
    m_window=window;m_floatingSize=window->size();m_presentation={};
    const auto epoch=++m_surfaceEpoch;
    m_frameFence=std::make_shared<WidgetFrameFence>();
    if(auto* quick=qobject_cast<QQuickWindow*>(window)){
        const auto fence=m_frameFence;
        connect(quick,&QQuickWindow::afterAnimating,this,&WidgetViewModel::commitRevealRegion);
        connect(quick,&QQuickWindow::afterSynchronizing,this,[fence]{const auto id=fence->pending.exchange(-1);if(id>=0)fence->synced=id;},Qt::DirectConnection);
        connect(quick,&QQuickWindow::frameSwapped,this,[this,fence,epoch]{
            const auto id=fence->synced.exchange(-1);
            if(id>=0)QMetaObject::invokeMethod(this,[this,id,epoch]{if(epoch==m_surfaceEpoch)finishTransition(id);},Qt::QueuedConnection);
        },Qt::DirectConnection);
    }
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
    const auto oldLayout=m_layout;const bool oldMotion=m_panelMotion,oldPending=m_pendingCollapse,oldVisual=m_visualPending;
    if(!m_pendingCollapse && !m_resizing && original!=m_appliedGeometry)captureUserGeometry();
    m_hoverTimer.stop();m_idleTimer.stop();m_returnTimer.stop();
    if(!candidate.enabled){
        if(!accept(m_platform.setVisible(m_window,false)))return false;
        m_presentation=candidate;m_pendingCollapse=false;m_panelMotion=false;m_visualPending=false;++m_transitionId;emit stateChanged();return true;
    }
    const bool visual=animate && m_visualTransitions && visible() && !clickThrough();
    m_visualPending=visual && (oldRender!=candidate.effective || oldVisual);
    m_presentation=candidate;++m_transitionId;m_revealFrom=oldRender==WidgetPresentation::Ball?0:1;
    m_pendingCollapse=visual && oldRender!=WidgetPresentation::Ball && candidate.effective==WidgetPresentation::Ball;
    m_panelMotion=visual && (oldRender!=candidate.effective || oldMotion) && (oldRender==WidgetPresentation::Panel || candidate.effective==WidgetPresentation::Panel || oldMotion);
    if(!m_pendingCollapse)m_renderForm=candidate.effective;
    m_geometryChanging=true;
    if(m_visualTransitions)m_layout=WidgetPlacementPolicy::canvasLayout(m_anchor,{formSize(0),formSize(1),formSize(2)},candidate.enabled,screens());
    else {
        m_layout.canvas=WidgetPlacementPolicy::resolve(m_anchor,formSize(m_renderForm),screens());
        for(int form=0;form<3;++form)m_layout.forms[form]=QRect(QPoint(),formSize(form));
        m_layout.forms[m_renderForm]=QRect(QPoint(),m_layout.canvas.size());
    }
    const auto target=m_layout.canvas;
    if(m_visualTransitions && target!=original){m_visualPending=false;m_pendingCollapse=false;m_panelMotion=false;m_renderForm=candidate.effective;}
    emit stateChanged();
    const auto result=target.size()==m_window->size()?WindowResult{}:m_platform.resize(m_window,target.size());
    if(result.succeeded()){
        if(m_window->position()!=target.topLeft())m_window->setPosition(target.topLeft());
        m_appliedGeometry=m_window->geometry();
    }else{
        m_presentation=previous;m_renderForm=oldRender;m_panelMotion=oldMotion;m_pendingCollapse=oldPending;m_visualPending=oldVisual;m_layout=oldLayout;
        if(m_window->geometry()!=original)m_window->setGeometry(original);
        m_appliedGeometry=original;
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
    if((mini() || (m_visualTransitions && m_window)) && !applyPresentation(m_presentation,false)){m_miniHeight=oldHeight;m_ballDiameter=oldDiameter;return false;}
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
    const bool held=m_pressed || m_menu || m_keyboard || m_moving || m_visualPending;
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
void WidgetViewModel::enableVisualTransitions(){if(m_visualTransitions)return;m_visualTransitions=true;if(m_window)applyPresentation(m_presentation,false);}
void WidgetViewModel::finishTransition(int id)
{
    if(id!=m_transitionId)return;
    if(!m_pendingCollapse && !m_panelMotion && !m_visualPending)return;
    m_pendingCollapse=false;m_panelMotion=false;m_visualPending=false;m_renderForm=m_presentation.effective;
    // Completion never mutates native geometry, mask, opacity or clipping.
    emit stateChanged();scheduleAutomatic();
}
void WidgetViewModel::presentTransition(int id)
{
    if(id!=m_transitionId)return;
    if(auto* quick=qobject_cast<QQuickWindow*>(m_window.data());quick && visible()){
        m_frameFence->pending=id;quick->update();
    }else finishTransition(id); // State-only QWindow fixtures have no render loop.
}
void WidgetViewModel::setPanelMinimumSize(int width,int height)
{
    const auto previous=formSize(0);const auto oldMinimum=m_panelMinimum;m_panelMinimum=QSize(qMax(260,width),qMax(190,height));
    if(m_window && formSize(0)!=previous && !applyPresentation(m_presentation,false))m_panelMinimum=oldMinimum;
}
void WidgetViewModel::setRevealRegion(const QRect& rectangle,double radius)
{
    if(!m_window || m_geometryChanging || rectangle.isEmpty())return;
    m_pendingRegion=rectangle;m_pendingRadius=radius;m_regionDirty=true;
    if(!qobject_cast<QQuickWindow*>(m_window.data()))commitRevealRegion();
}
void WidgetViewModel::commitRevealRegion()
{
    if(!m_regionDirty || !m_window || m_geometryChanging)return;m_regionDirty=false;
    if(QGuiApplication::platformName()==QStringLiteral("offscreen") || QGuiApplication::platformName()==QStringLiteral("minimal"))return;
    const auto rect=m_pendingRegion.intersected(QRect(QPoint(),m_window->size()));if(rect.isEmpty())return;
    QPainterPath path;const auto radius=qBound(0.,m_pendingRadius,qMin(rect.width(),rect.height())/2.);
    path.addRoundedRect(QRectF(rect),radius,radius);const QRegion region(path.toFillPolygon().toPolygon());
    if(m_window->mask()!=region)m_window->setMask(region);
}
bool WidgetViewModel::setAlwaysOnTop(bool enabled){const bool ok=accept(m_platform.setAlwaysOnTop(m_window,enabled));emit stateChanged();return ok;}
bool WidgetViewModel::setDesktopPinned(bool enabled){const bool ok=accept(m_platform.setDesktopPinned(m_window,enabled));emit stateChanged();return ok;}
bool WidgetViewModel::setLayer(int value){if(value<0 || value>2)return accept({WindowError::OperationRejected});return value==2?setDesktopPinned(true):setAlwaysOnTop(value==1);}
bool WidgetViewModel::setVisible(bool value)
{
    if(!value)finishMove();
    if(value && !m_presentation.enabled){emit configurationRequested();accept({WindowError::OperationRejected});emit stateChanged();return false;}
    if(value){auto candidate=m_presentation;candidate.returnToSelected();if(!applyPresentation(candidate,false))return false;}
    if(value)commitRevealRegion();
    if(!accept(m_platform.setVisible(m_window,value))){emit stateChanged();return false;}
    m_requestedVisible=value;if(!value){m_hoverTimer.stop();m_idleTimer.stop();m_returnTimer.stop();m_pendingCollapse=false;m_panelMotion=false;m_visualPending=false;++m_transitionId;}
    emit stateChanged();scheduleAutomatic();return true;
}
bool WidgetViewModel::beginPointerMove(const QPointF& globalPress)
{
    if(m_locked || !m_window || !visible() || clickThrough() || !std::isfinite(globalPress.x()) || !std::isfinite(globalPress.y()))return accept({WindowError::OperationRejected});
    m_pointerPress=globalPress;m_pointerOrigin=m_window->position();m_pointerMoving=true;m_moving=true;
    m_hoverTimer.stop();m_idleTimer.stop();m_returnTimer.stop();m_moveTimer.stop();return accept({});
}
bool WidgetViewModel::movePointer(const QPointF& globalPosition)
{
    if(!m_pointerMoving || !m_moving || m_locked || !m_window || !std::isfinite(globalPosition.x()) || !std::isfinite(globalPosition.y()))return false;
    const auto result=m_platform.move(m_window,m_pointerOrigin+(globalPosition-m_pointerPress).toPoint());
    if(!result.succeeded()){finishMove();return accept(result);}return true;
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
    if(m_visualTransitions){m_resizing=true;m_moving=true;m_resizeCursor=QCursor::pos();m_resizeOrigin=m_layout.forms[0].translated(m_window->position());m_moveTimer.start();return accept({});}
    const auto result=m_platform.startResize(m_window);if(result.succeeded()){m_moving=true;m_moveTimer.start();}return accept(result);
}
void WidgetViewModel::resizePanel(const QPointF& globalPosition)
{
    if(!m_resizing || !m_window)return;
    const auto delta=(globalPosition-m_resizeCursor).toPoint();
    auto rect=m_resizeOrigin;rect.setSize((m_resizeOrigin.size()+QSize(delta.x(),delta.y())).expandedTo(m_panelMinimum));
    const auto available=screens();const int index=WidgetPlacementPolicy::screenFor(rect,available);if(index<0)return;
    const auto screen=available[index];const auto bounds=screen.available;
    const QPoint at(m_anchor.right?rect.x()+rect.width():rect.x(),m_anchor.bottom?rect.y()+rect.height():rect.y());
    const auto oldSize=m_floatingSize;const auto oldAnchor=m_anchor;
    m_floatingSize=rect.size();m_anchor={screen.id,bounds,{double(at.x()-bounds.x())/qMax(1,bounds.width()),double(at.y()-bounds.y())/qMax(1,bounds.height())},oldAnchor.right,oldAnchor.bottom};
    if(!applyPresentation(m_presentation,false)){m_floatingSize=oldSize;m_anchor=oldAnchor;}
}
void WidgetViewModel::finishMove(){m_pointerMoving=false;m_moveTimer.stop();if(m_moving){m_resizing=false;captureUserGeometry();m_moving=false;normalizeGeometry();emit stateChanged();scheduleAutomatic();}}
void WidgetViewModel::configureSettings(const QString& path){m_settings=std::make_unique<QSettings>(path,QSettings::IniFormat);}
void WidgetViewModel::setTrayAvailable(bool available){if(!available && clickThrough() && !setClickThrough(false))setVisible(false);m_trayAvailable=available;emit stateChanged();}
bool WidgetViewModel::setClickThrough(bool enabled)
{
    if(enabled)finishMove();
    if(enabled && !m_trayAvailable)return accept({WindowError::OperationRejected});
    if(enabled && (m_pendingCollapse || m_panelMotion) && !applyPresentation(m_presentation,false))return false;
    const bool ok=accept(m_platform.setClickThrough(m_window,enabled));emit stateChanged();scheduleAutomatic();return ok;
}
void WidgetViewModel::setLocked(bool locked){if(locked)finishMove();m_locked=locked;emit stateChanged();}
void WidgetViewModel::setBackgroundOpacity(double opacity){m_backgroundOpacity=std::isfinite(opacity)?qBound(0.6,opacity,1.0):0.9;emit stateChanged();}
void WidgetViewModel::setCloseToTray(bool enabled){m_closeToTray=enabled;emit stateChanged();}
void WidgetViewModel::normalizeGeometry()
{
    if(!m_window || m_geometryChanging)return;
    if(m_window->geometry()!=m_appliedGeometry)captureUserGeometry();
    applyPresentation(m_presentation,false);
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

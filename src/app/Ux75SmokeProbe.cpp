#include "Ux75SmokeProbe.h"
#include "models/TaskWorkspace.h"
#include "models/WidgetViewModel.h"
#include "platform/common/NativeWindowLayer.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QGuiApplication>
#include <QScreen>
#include <QSet>
#include <QDebug>
namespace daymark {
Ux75SmokeProbe::Ux75SmokeProbe(TaskWorkspace& workspace,WidgetViewModel& controller,QQuickWindow* main,QQuickWindow* widget,QString screenshot)
    :m_workspace(workspace),m_controller(controller),m_main(main),m_widget(widget),m_screenshot(std::move(screenshot)){
    m_wait.start();
    QObject::connect(widget,&QQuickWindow::afterSynchronizing,this,[this]{++m_syncedFrames;},Qt::QueuedConnection);
    QObject::connect(widget,&QQuickWindow::frameSwapped,this,[this]{++m_swappedFrames;},Qt::QueuedConnection);
    QObject::connect(widget,&QQuickWindow::afterFrameEnd,this,[this]{++m_endedFrames;},Qt::QueuedConnection);
    QObject::connect(widget,&QWindow::widthChanged,this,[this]{++m_geometryEvents;});
    QObject::connect(widget,&QWindow::heightChanged,this,[this]{++m_geometryEvents;});
    QObject::connect(widget,&QQuickWindow::afterAnimating,this,[this]{
        if(m_monitor && m_widget->property("motionProgress").toDouble()>0.05 && m_widget->property("motionProgress").toDouble()<0.95)++m_intermediateFrames;
        if(m_monitor && QGuiApplication::platformName()=="windows" && m_widget->mask().isEmpty())m_emptyMask=true;
    });
    QObject::connect(widget,&QQuickWindow::sceneGraphInvalidated,this,[this]{if(m_monitor)m_lostGraph=true;});
}
QObject* Ux75SmokeProbe::find(QObject* root,const QString& name)const{
    QList<QObject*> pending{root};QSet<QObject*> seen;
    while(!pending.isEmpty()){
        auto* object=pending.takeLast();if(!object || seen.contains(object))continue;seen.insert(object);
        if(object->objectName()==name)return object;
        pending.append(object->children());
        if(auto* item=qobject_cast<QQuickItem*>(object))for(auto* child:item->childItems())pending.append(child);
        if(auto* window=qobject_cast<QQuickWindow*>(object))pending.append(window->contentItem());
    }
    return nullptr;
}
QString Ux75SmokeProbe::advance(){
    if(m_stage==9)return "DONE";
    if(m_wait.elapsed()<100)return {};
    const auto fail=[&](const QString& reason){return QStringLiteral("UX75 stage %1 cycle %2 target %3: %4").arg(m_stage).arg(m_cycle).arg(m_target).arg(reason);};
    static constexpr int targets[]={2,0,1,2,1,0};
    if(m_stage==8){
        const auto frame=m_main->grabWindow().convertToFormat(QImage::Format_RGBA8888);
        if(frame.isNull())return fail("restored main frame missing");
        const double sx=double(frame.width())/m_main->width(),sy=double(frame.height())/m_main->height();
        for(const auto* name:{"settingsAndTools","navigation-trash","showCalendarView"}){
            auto* control=find(m_main,QString::fromLatin1(name));
            auto* glyph=control?qobject_cast<QQuickItem*>(control->property("contentItem").value<QObject*>()):nullptr;
            if(!glyph || !glyph->isVisible())return fail(QStringLiteral("restored glyph missing: %1").arg(QString::fromLatin1(name)));
            const auto ink=glyph->property("ink").value<QColor>();
            const auto at=glyph->mapToScene(QPointF());
            const auto area=QRect(qRound(at.x()*sx),qRound(at.y()*sy),qRound(glyph->width()*sx),qRound(glyph->height()*sy)).intersected(frame.rect());
            if(!ink.isValid() || area.isEmpty())return fail("restored glyph has invalid paint geometry");
            int painted=0;
            for(int y=area.top();y<=area.bottom();++y)for(int x=area.left();x<=area.right();++x){const auto pixel=frame.pixelColor(x,y);if(qAbs(pixel.red()-ink.red())<24 && qAbs(pixel.green()-ink.green())<24 && qAbs(pixel.blue()-ink.blue())<24 && pixel.alpha()>128)++painted;}
            if(painted<3)return fail(QStringLiteral("restored operation icon ink mismatch: %1").arg(QString::fromLatin1(name)));
        }
        if(!m_screenshot.isEmpty() && !frame.save(m_screenshot+".restored-icons.png"))return fail("restored glyph frame save");
        m_stage=9;return "DONE";
    }
    if(m_stage==0){
        m_workspace.setPreference("followSystemAccessibility",false);m_workspace.setPreference("reduceMotion",false);
        m_workspace.setPreference("solidWidget",true);
        m_workspace.setPreference("desktopCalendarVisible",true);
        if(!m_workspace.createTodayTask(QStringLiteral("绘制事务验证")))return fail("fixture create");
        m_stage=1;
    }else if(m_stage==1){
        m_controller.setVisible(false);m_controller.setFormEnabled(0,true);m_controller.setFormEnabled(1,true);m_controller.setFormEnabled(2,true);
        m_controller.setAutoCompact(false);m_controller.selectForm(0);
        m_widget->resize(420,400);m_controller.normalizeGeometry();m_controller.setVisible(true);m_main->hide();m_stage=2;
    }else if(m_stage==2){
        m_monitor=false;m_workspace.setThemeMode(m_cycle?2:1);m_workspace.setTextScale(m_cycle?2:1);
        const auto available=m_widget->screen()->availableGeometry();
        m_widget->setPosition(m_cycle?available.bottomRight()-QPoint(m_widget->width()+24,m_widget->height()+24):available.topLeft()+QPoint(24,24));
        m_controller.normalizeGeometry();m_widget->raise();m_widget->requestActivate();m_widget->update();m_stage=3;
    }else if(m_stage==3){
        if(QGuiApplication::platformName()=="windows" && !m_widget->isExposed()){if(m_wait.elapsed()<1200){m_widget->raise();m_widget->update();return {};}return fail("rendering target is not exposed");}
        if(m_controller.transitionPending()){if(m_wait.elapsed()<1200){m_widget->update();return {};}return fail("initial layout not presented");}
        m_canvas=m_widget->geometry();m_geometryBaseline=m_geometryEvents;m_monitor=true;m_emptyMask=false;m_lostGraph=false;m_stage=4;
    }else if(m_stage==4){
        if(!m_controller.selectForm(targets[m_target]))return fail("form command rejected");
        m_widget->update();m_stage=5;
    }else if(m_stage==5){
        if(m_controller.transitionPending() || m_widget->property("motionInputBlocked").toBool()){
            if(m_wait.elapsed()<1500){m_widget->update();return {};}return fail(QStringLiteral("presented completion fence did not finish: exposed=%1 graph=%2 visible=%3 progress=%4 pending=%5 sync=%6 swap=%7 end=%8 visualId=%9 controllerId=%10 lostGraph=%11").arg(m_widget->isExposed()).arg(m_widget->isSceneGraphInitialized()).arg(m_widget->isVisible()).arg(m_widget->property("motionProgress").toDouble()).arg(m_controller.transitionPending()).arg(m_syncedFrames).arg(m_swappedFrames).arg(m_endedFrames).arg(m_widget->property("visualTransition").toInt()).arg(m_controller.transitionId()).arg(m_lostGraph));
        }
        if(m_widget->geometry()!=m_canvas || m_geometryEvents!=m_geometryBaseline)return fail("shape change resized/repositioned native canvas");
        if(m_emptyMask || m_lostGraph)return fail("shape change cleared mask or invalidated scene graph");
        if(targets[m_target]==0 && qAbs(m_widget->property("panelContentOpacity").toDouble()-1)>0.001)return fail("stable Panel content is faded");
        const auto expected=targets[m_target]==0?m_controller.panelRect():targets[m_target]==1?m_controller.miniRect():m_controller.ballRect();
        auto* surface=qobject_cast<QQuickItem*>(find(m_widget,"widgetBackground"));
        if(!surface || (surface->position()-expected.topLeft()).manhattanLength()>1 || qAbs(surface->width()-expected.width())>1 || qAbs(surface->height()-expected.height())>1)return fail("visual rectangle differs from logical target");
        if(targets[m_target]==1){
            auto* more=qobject_cast<QQuickItem*>(find(m_widget,"miniMore"));
            if(!more || !expected.contains(more->mapRectToScene(more->boundingRect()).toAlignedRect()))
                return fail("Mini menu button does not fit inside the strip");
            if(QGuiApplication::platformName()=="windows"){
                const auto image=m_widget->grabWindow();
                if(image.isNull())return fail("Mini frame missing");
                const double sx=double(image.width())/m_widget->width();
                const double sy=double(image.height())/m_widget->height();
                const auto corner=QRect(qRound(expected.x()*sx),qRound(expected.y()*sy),
                                        qRound(14*sx),qRound(14*sy)).intersected(image.rect());
                bool antialiasedEdge=false;
                for(int y=corner.top();y<=corner.bottom();++y){
                    for(int x=corner.left();x<=corner.right();++x){
                        const auto alpha=image.pixelColor(x,y).alpha();
                        if(alpha>0 && alpha<255){
                            if(!m_widget->mask().contains(QPoint(int(x/sx),int(y/sy))))
                                return fail("Mini antialiased pixels extend outside the native region");
                            antialiasedEdge=true;
                        }
                    }
                }
                if(!antialiasedEdge)return fail("Mini corner has no antialiasing");
            }
        }
        if(targets[m_target]==2){m_samples=0;m_ballFrame={};m_stage=6;}else m_stage=7;
    }else if(m_stage==6){
        const auto image=m_widget->grabWindow().convertToFormat(QImage::Format_RGBA8888);
        if(image.isNull())return fail("empty terminal frame");
        const auto ball=m_controller.ballRect();const double sx=double(image.width())/m_widget->width(),sy=double(image.height())/m_widget->height();
        const auto center=QPoint(qRound(ball.center().x()*sx),qRound(ball.center().y()*sy));
        if(!image.rect().contains(center) || image.pixelColor(center).alpha()<64)return fail("terminal ball was not painted");
        const auto far=QPoint(ball.center().x()<m_widget->width()/2?image.width()-2:1,ball.center().y()<m_widget->height()/2?image.height()-2:1);
        if(image.pixelColor(far).alpha()!=0)return fail("retired panel pixels remain in transparent canvas");
        if(m_samples && image!=m_ballFrame){if(!m_screenshot.isEmpty()){m_ballFrame.save(m_screenshot+".before.png");image.save(m_screenshot+".after.png");}return fail("terminal ball changes on a later frame");}
        m_ballFrame=image;
        if(QGuiApplication::platformName()=="windows"){
            const auto mask=m_widget->mask().boundingRect();if(!nativeWindowRegionMatches(m_widget,mask))return fail("Win32 region differs from Qt mask");if(qAbs(mask.x()-ball.x())>1 || qAbs(mask.y()-ball.y())>1 || qAbs(mask.width()-ball.width())>1 || qAbs(mask.height()-ball.height())>1)return fail("native input region does not cover only the ball");
        }
        if(++m_samples==3){if(!m_screenshot.isEmpty() && !image.save(m_screenshot+QStringLiteral(".ball-%1-%2.png").arg(m_cycle).arg(m_target)))return fail("frame save");m_stage=7;}
        m_widget->update();
    }else if(m_stage==7){
        qInfo()<<"Stable rendering"<<m_cycle<<targets[m_target]<<m_canvas<<"geometry events"<<m_geometryEvents-m_geometryBaseline;
        if(++m_target<6)m_stage=4;
        else if(++m_cycle<2){m_target=0;m_monitor=false;m_stage=2;}
        else {if(!m_intermediateFrames)return fail("no actual interpolated frame was observed");m_monitor=false;m_main->show();m_stage=8;}
    }
    m_wait.restart();return {};
}
}

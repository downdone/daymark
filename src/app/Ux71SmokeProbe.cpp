#include "Ux71SmokeProbe.h"
#include "models/TaskWorkspace.h"
#include "models/WidgetViewModel.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QSet>
#include <QImage>
#include <QDebug>
namespace daymark {
Ux71SmokeProbe::Ux71SmokeProbe(TaskWorkspace& w,WidgetViewModel& c,QQuickWindow* main,QQuickWindow* widget,QString shot)
    :m_workspace(w),m_controller(c),m_main(main),m_widget(widget),m_screenshot(std::move(shot)){
    m_wait.start();m_clock.start();
    QObject::connect(main,&QQuickWindow::afterAnimating,main,[this]{
        if(m_stage<33 || m_stage>54 || m_opacity<0)return;
        auto* bar=find(m_main,"taskScrollBar");auto* thumb=bar?bar->property("contentItem").value<QObject*>():nullptr;
        if(thumb && qAbs(thumb->property("opacity").toDouble()-m_opacity)>0.01)m_flickered=true;
    });
}
QObject* Ux71SmokeProbe::find(QObject* root,const QString& name) const {
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
void Ux71SmokeProbe::click(QQuickItem* item,QQuickWindow* receiver){
    if(!item)return;
    auto* window=receiver?receiver:item->window();const auto global=item->mapToGlobal(QPointF(item->width()/2,item->height()/2));const auto at=window->mapFromGlobal(global);
    for(const auto type:{QEvent::MouseMove,QEvent::MouseButtonPress,QEvent::MouseButtonRelease}){
        QMouseEvent event(type,at,global,type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonPress?Qt::LeftButton:Qt::NoButton,Qt::NoModifier);
        QCoreApplication::sendEvent(window,&event);
    }
}
bool Ux71SmokeProbe::capture(QQuickWindow* window,const QString& name) const {
    return m_screenshot.isEmpty() || (window && window->grabWindow().save(m_screenshot+".input-"+name+".png"));
}
void Ux71SmokeProbe::rightClickBlank(){
    const QPointF at(40,m_widget->height()*0.65);
    for(const auto type:{QEvent::MouseMove,QEvent::MouseButtonPress,QEvent::MouseButtonRelease}){
        QMouseEvent event(type,at,m_widget->mapToGlobal(at),type==QEvent::MouseMove?Qt::NoButton:Qt::RightButton,type==QEvent::MouseButtonPress?Qt::RightButton:Qt::NoButton,Qt::NoModifier);
        QCoreApplication::sendEvent(m_widget,&event);
    }
}
QString Ux71SmokeProbe::advance(){
    if(m_stage==99)return "DONE";
    if(m_wait.elapsed()<100)return {};
    // A native runner can be occluded; force the same layout/presentation
    // synchronization used by the other probes before delivering wheel input.
    if(m_stage>=32 && m_stage<=56)m_main->grabWindow();
    const auto fail=[&](const QString& reason){capture(m_main,"failure");return QStringLiteral("UX71 stage %1: %2").arg(m_stage).arg(reason);};
    auto* list=qobject_cast<QQuickItem*>(find(m_main,"taskList"));auto* bar=find(m_main,"taskScrollBar");
    if(!list || !bar)return fail("list/scrollbar missing");
    if(m_stage==0){
        m_workspace.setPreference("followSystemAccessibility",false);m_workspace.setPreference("reduceMotion",true);m_workspace.setPreference("desktopCalendarVisible",true);
        m_workspace.setThemeMode(1);m_workspace.setTextScale(1);m_workspace.setView("all");m_main->resize(1100,640);m_controller.setVisible(false);
    }
    if(m_stage>=1 && m_stage<=30 && !m_workspace.createTodayTask(QStringLiteral("滚轮与菜单验证 %1").arg(m_stage)))return fail("fixture create");
    if(m_stage==31){
        QVariantList selection;const auto tasks=m_workspace.tasks()->items();
        for(int i=1;i<tasks.size();++i)selection.append(QVariantMap{{"taskId",tasks[i].id},{"rowVersion",tasks[i].rowVersion}});
        if(tasks.size()!=30 || !m_workspace.batchTasks(selection,"delete",{}))return fail("trash fixture");
    }
    if(m_stage==32)m_workspace.setView("trash");
    if(m_stage>=33 && m_stage<=54){
        const int phase=(m_stage-33)%11;
        if(phase==0){
            m_workspace.setThemeMode(m_stage>=44?2:1);m_workspace.setTextScale(m_stage>=44?2:1);
            list->setProperty("contentY",list->property("originY"));m_opacity=-1;
            m_main->show();m_main->requestActivate();list->forceActiveFocus();
        }else if(phase==10){
            if(list->property("contentY").toDouble()<=m_beforeY || !bar->property("visible").toBool())return fail("real wheel did not scroll overflowing trash");
            if(!capture(m_main,m_stage>=44?"trash-dark-200":"trash-light"))return fail("capture");
        }else {
            if(phase%2==1){
                const double opacity=bar->property("contentItem").value<QObject*>()->property("opacity").toDouble();
                if(m_opacity>=0 && qAbs(opacity-m_opacity)>0.01)m_flickered=true;
                m_opacity=opacity;m_beforeY=list->property("contentY").toDouble();
                const auto at=list->mapToScene(QPointF(list->width()/2,list->height()/2));
                QMouseEvent move(QEvent::MouseMove,at,m_main->mapToGlobal(at),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(m_main,&move);
                QWheelEvent wheel(at,m_main->mapToGlobal(at),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                wheel.setTimestamp(static_cast<quint64>(m_clock.elapsed()));
                QCoreApplication::sendEvent(m_main,&wheel);
                qInfo()<<"Wheel delivery"<<wheel.isAccepted()<<m_main->isActive()<<list->property("interactive")<<list->property("contentY")<<list->property("contentHeight");
            }else{
                const auto opacity=bar->property("contentItem").value<QObject*>()->property("opacity").toDouble();
                qInfo()<<"Wheel feedback"<<m_stage<<bar->property("active")<<opacity;
                if(qAbs(opacity-m_opacity)>0.01)m_flickered=true;
            }
        }
    }
    if(m_stage==55){
        // Real users perform these operations across presented frames. Do not
        // recreate a hidden native surface, resize it and reverse its form in
        // one event turn while the previous window's theme is still changing.
        static int setup=0;
        if(setup==0){m_workspace.setThemeMode(1);m_workspace.setTextScale(1);m_workspace.setView("today");}
        if(setup==1){
            m_main->hide();m_controller.setFormEnabled(0,true);m_controller.setFormEnabled(2,true);m_controller.setFormEnabled(1,false);
            m_controller.setAutoCompact(false);m_controller.selectForm(0);m_controller.setLayer(1);m_widget->resize(400,420);m_widget->setPosition(200,120);m_controller.setVisible(true);m_widget->grabWindow();
        }
        if(setup==2){m_controller.selectForm(2);m_widget->grabWindow();}
        if(setup==3){m_controller.activatePrimary();m_widget->grabWindow();}
        qInfo()<<"Widget input fixture presented setup"<<setup;
        if(++setup<4){m_wait.restart();return {};}
    }
    if(m_stage==56){
        if(bar->property("visible").toBool() || list->property("interactive").toBool())return fail("nonoverflowing list remains scrollable");
        QMetaObject::invokeMethod(m_widget,"openVerificationMenu");
    }
    if(m_stage==57){
        auto* menu=find(m_widget,"widgetMenu");auto* item=qobject_cast<QQuickItem*>(find(m_widget,"widgetFormPanel"));
        if(!menu || !menu->property("visible").toBool() || !item || item->window()==m_widget)return fail("native menu missing");
        if(!capture(item->window(),"menu-input"))return fail("menu capture");
        // Replay a host-routed click at the same global point. It must never
        // activate the task underneath, even when native capture is interrupted.
        click(item,m_widget);
    }
    if(m_stage==58){
        m_leaked=m_main->isVisible() || !m_main->property("editId").toString().isEmpty();
        if(m_leaked){QMetaObject::invokeMethod(m_main,"returnFromDetails");m_main->hide();}
        auto* menu=find(m_widget,"widgetMenu");
        if(!menu->property("visible").toBool())QMetaObject::invokeMethod(m_widget,"openVerificationMenu");
    }
    if(m_stage==59)click(qobject_cast<QQuickItem*>(find(m_widget,"widgetFormPanel")));
    if(m_stage==60){
        if(m_controller.selectedForm()!=0 || find(m_widget,"widgetMenu")->property("visible").toBool() || m_main->isVisible())return fail("menu selection opens a task instead of selecting Panel");
        if(m_leaked || m_flickered)return fail(QStringLiteral("reproduced regressions: scrollbar=%1 backgroundTask=%2").arg(m_flickered).arg(m_leaked));
        m_workspace.setThemeMode(2);m_workspace.setTextScale(2);QMetaObject::invokeMethod(m_widget,"openVerificationMenu");
    }
    if(m_stage==61){click(qobject_cast<QQuickItem*>(find(m_widget,"widgetFormBall")));}
    if(m_stage==62){
        if(m_controller.selectedForm()!=2 || m_main->isVisible())return fail("dark 200 percent menu command");
        m_controller.activatePrimary();
    }
    if(m_stage==63){
        auto* row=qobject_cast<QQuickItem*>(find(m_widget,"widgetTask-"+m_workspace.todayTasks()->taskIdAt(0)));
        click(row);
    }
    if(m_stage==64){
        if(!m_main->isVisible() || m_main->property("editId").toString().isEmpty())return fail("task input does not recover after dismissal");
        QMetaObject::invokeMethod(m_main,"returnFromDetails");m_controller.setVisible(true);m_main->show();
    }
    if(m_stage==65){
        m_main->hide();m_workspace.setThemeMode(1);m_workspace.setTextScale(1);
        m_controller.selectForm(0);m_controller.setVisible(true);m_widget->requestActivate();
    }
    if(m_stage>=66 && m_stage<=97){
        const int cycle=(m_stage-66)/4,part=(m_stage-66)%4;
        auto* menu=find(m_widget,"widgetMenu");
        if(part==0){
            // Theme, text scale and native geometry must have presented on a
            // separate frame before taking the lifecycle's reference image.
            m_blank=m_widget->grabWindow().copy(QRect(22,240,40,30)).convertToFormat(QImage::Format_RGBA8888);
            if(m_blank.isNull())return fail("background reference capture is empty");
            rightClickBlank();
        }
        if(part==1){
            if(!menu->property("visible").toBool())return fail("blank-area context menu did not remain open");
            auto* menuItem=qobject_cast<QQuickItem*>(menu->property("contentItem").value<QObject*>());
            if(!menuItem || menuItem->window()==m_widget)return fail("context menu lost independent native surface");
            m_menuWindow=menuItem->window();
            const auto blank=m_widget->grabWindow().copy(QRect(22,240,40,30)).convertToFormat(QImage::Format_RGBA8888);
            if(blank!=m_blank)return fail("opening a menu changes the blank background");
            auto* row=qobject_cast<QQuickItem*>(find(m_widget,"widgetTask-"+m_workspace.todayTasks()->taskIdAt(0)));
            if(!row || !row->isEnabled())return fail("menu changes the task's visual enabled state");
            if(!capture(menuItem->window(),QStringLiteral("lifecycle-%1-open").arg(cycle)))return fail("lifecycle capture");
        }
        if(part==2){
            // Qt's native Popup can forward the outside gesture to its host
            // after closing. Reproduce both parts in the same dispatch turn.
            QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");
            if(m_menuWindow && m_menuWindow->color().alpha()!=255)return fail("closing the menu restores a transparent native clear color");
            if(m_widget->color().alpha()!=0)return fail("menu mutates transparent host clear color");
            rightClickBlank();
            if(menu->property("visible").toBool())return fail("dismissal right-click reopens the menu in the same gesture");
        }
        if(part==3){
            if(menu->property("visible").toBool())return fail("dismissed menu reopened on the next frame");
            const auto blank=m_widget->grabWindow().copy(QRect(22,240,40,30)).convertToFormat(QImage::Format_RGBA8888);
            if(blank!=m_blank){
                if(!m_screenshot.isEmpty()){m_blank.save(m_screenshot+".blank-before.png");blank.save(m_screenshot+".blank-after.png");}
                return fail("blank background changed across context-menu lifecycle");
            }
            if(cycle==3){m_workspace.setThemeMode(2);m_workspace.setTextScale(2);}
        }
    }
    if(m_stage==98){m_main->show();m_controller.setVisible(true);}
    ++m_stage;m_wait.restart();return m_stage==99?QStringLiteral("DONE"):QString();
}
}

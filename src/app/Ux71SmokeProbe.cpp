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
    :m_workspace(w),m_controller(c),m_main(main),m_widget(widget),m_screenshot(std::move(shot)){m_wait.start();}
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
QString Ux71SmokeProbe::advance(){
    if(m_stage==65)return "DONE";
    if(m_wait.elapsed()<100)return {};
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
        }else if(phase==10){
            if(list->property("contentY").toDouble()<=m_beforeY || !bar->property("visible").toBool())return fail("real wheel did not scroll overflowing trash");
            if(!capture(m_main,m_stage>=44?"trash-dark-200":"trash-light"))return fail("capture");
        }else {
            if(phase%2==1){
                const double opacity=bar->property("contentItem").value<QObject*>()->property("opacity").toDouble();
                if(m_opacity>=0 && qAbs(opacity-m_opacity)>0.01)m_flickered=true;
                m_opacity=opacity;m_beforeY=list->property("contentY").toDouble();
                const auto at=list->mapToScene(QPointF(list->width()/2,list->height()/2));
                QWheelEvent wheel(at,m_main->mapToGlobal(at),{},QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                QCoreApplication::sendEvent(m_main,&wheel);
            }else{
                const auto opacity=bar->property("contentItem").value<QObject*>()->property("opacity").toDouble();
                qInfo()<<"Wheel feedback"<<m_stage<<bar->property("active")<<opacity;
                if(qAbs(opacity-m_opacity)>0.01)m_flickered=true;
            }
        }
    }
    if(m_stage==55){
        m_workspace.setThemeMode(1);m_workspace.setTextScale(1);m_workspace.setView("today");
        m_main->hide();m_controller.setFormEnabled(0,true);m_controller.setFormEnabled(2,true);m_controller.setFormEnabled(1,false);
        m_controller.setAutoCompact(false);m_controller.setLayer(1);m_controller.selectForm(0);m_controller.setVisible(true);m_widget->resize(400,420);m_widget->setPosition(200,120);
        m_controller.selectForm(2);m_controller.activatePrimary();
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
    ++m_stage;m_wait.restart();return m_stage==65?QStringLiteral("DONE"):QString();
}
}

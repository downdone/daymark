#include "Ux76SmokeProbe.h"
#include "models/TaskWorkspace.h"
#include "models/WidgetViewModel.h"
#include "platform/common/SmokeMenuControl.h"
#include "platform/common/NativeWindowLayer.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QSet>
#include <QDebug>
namespace daymark {
Ux76SmokeProbe::Ux76SmokeProbe(TaskWorkspace& workspace,WidgetViewModel& controller,QQuickWindow* main,QQuickWindow* widget,QString screenshot)
    :m_workspace(workspace),m_controller(controller),m_main(main),m_widget(widget),m_screenshot(std::move(screenshot)){
    m_wait.start();
    m_native=QGuiApplication::platformName()=="windows" && !qEnvironmentVariableIsSet("DAYMARK_SMOKE_MENU_FALLBACK");
}
QObject* Ux76SmokeProbe::find(const QString& name)const{
    QList<QObject*> pending{m_widget};QSet<QObject*> seen;
    while(!pending.isEmpty()){
        auto* object=pending.takeLast();if(!object || seen.contains(object))continue;
        seen.insert(object);
        if(object->objectName()==name){
            auto* item=qobject_cast<QQuickItem*>(object);
            if(!item || (item->isVisible() && item->window()))return object;
        }
        pending.append(object->children());
        if(auto* item=qobject_cast<QQuickItem*>(object))for(auto* child:item->childItems())pending.append(child);
        if(auto* window=qobject_cast<QQuickWindow*>(object))pending.append(window->contentItem());
    }
    return nullptr;
}
void Ux76SmokeProbe::pointer(QQuickItem* item,Qt::MouseButton button){
    if(!item || !item->window()){m_error="missing presented pointer target";return;}
    auto* receiver=item->window();
    const auto at=item->mapToScene(QPointF(qMin(70.,item->width()/2),item->height()/2));
    const auto global=receiver->mapToGlobal(at);
    for(const auto type:{QEvent::MouseMove,QEvent::MouseButtonPress,QEvent::MouseButtonRelease}){
        QMouseEvent event(type,at,global,type==QEvent::MouseMove?Qt::NoButton:button,type==QEvent::MouseButtonPress?button:Qt::NoButton,Qt::NoModifier);
        QCoreApplication::sendEvent(receiver,&event);
    }
}
void Ux76SmokeProbe::checkMenu(){
    auto* menu=find("widgetTaskMenu");
    const auto context=m_widget->property("contextTask").toMap();
    qInfo()<<"Task menu state"<<m_stage<<"visible"<<(menu?menu->property("visible"):QVariant())<<"blocked"<<m_widget->property("commandInputBlocked")<<"motion"<<m_widget->property("motionInputBlocked")<<"host active"<<m_widget->isActive()<<"focus"<<(m_widget->activeFocusItem()?m_widget->activeFocusItem()->objectName():QString());
    if(!m_screenshot.isEmpty())m_widget->grabWindow().save(m_screenshot+QStringLiteral(".host-%1-%2.png").arg(m_cycle).arg(m_stage));
    if(!menu || !menu->property("visible").toBool() || context.value("taskId").toString()!=m_tasks[0]){m_error="right-click did not open menu for hit task";return;}
    if(m_native){
        if(!smokeNativeMenuVisible()){m_error="requested Native but no actual Win32 menu is visible";return;}
        if(!m_controller.setLayer(m_controller.layer())){m_error="host layer restore failed";return;}
        if(!smokeNativeMenuAbove(m_widget)){m_error="native task menu is below its transparent host";return;}
    }else{
        auto* content=qobject_cast<QQuickItem*>(menu->property("contentItem").value<QObject*>());
        auto* window=content?content->window():nullptr;
        if(!window || window==m_widget || window->color().alpha()!=255 || window->opacity()!=1){
            qInfo()<<"Menu backend"<<menu->property("popupType")<<"Win32 menu"<<smokeNativeMenuVisible()<<"window"<<window<<"host"<<m_widget.data();
            m_error="task menu has no independent opaque window";return;
        }
        if(QGuiApplication::platformName()=="windows" && !nativeWindowAbove(window,m_widget)){m_error="QML menu is below its host";return;}
        for(auto* item=content;item;item=item->parentItem())if(!item->isEnabled() || qAbs(item->opacity()-1)>0.001){m_error="task menu inherits fading or disabled ancestor";return;}
        const auto image=window->grabWindow();
        if(image.isNull() || image.pixelColor(image.width()/2,image.height()/2).alpha()!=255){m_error="task menu rendered pixels are transparent";return;}
        if(!m_screenshot.isEmpty() && !image.save(m_screenshot+QStringLiteral(".menu-%1-%2.png").arg(m_cycle).arg(m_stage)))m_error="menu screenshot failed";
    }
    m_observed=true;
    qInfo()<<"Task menu"<<m_cycle<<m_stage<<"native"<<m_native<<"context"<<context.value("taskId");
}
QString Ux76SmokeProbe::advance(){
    if(!m_error.isEmpty()){QMetaObject::invokeMethod(find("widgetTaskMenu"),"close");return QStringLiteral("UX76 stage %1 cycle %2: %3").arg(m_stage).arg(m_cycle).arg(m_error);}
    if(m_stage==15)return "DONE";
    if(m_wait.elapsed()<120)return {};
    if(m_stage==0){
        m_workspace.setPreference("followSystemAccessibility",false);m_workspace.setPreference("reduceMotion",true);m_workspace.setPreference("desktopCalendarVisible",true);
        m_controller.setVisible(false);m_workspace.setThemeMode(1);m_workspace.setTextScale(1);
    }
    if(m_stage<=2){if(!m_workspace.createTodayTask(QStringLiteral("任务菜单 %1").arg(m_stage)))return "fixture create failed";}
    if(m_stage==3){
        for(const auto& task:m_workspace.todayTasks()->items())m_tasks.append(task.id);
        if(m_tasks.size()!=3)return "fixture task count";
        if(!m_native)for(const auto& name:{"widgetTaskMenu","widgetMenu"}){
            auto* menu=find(name);if(!menu || !QMetaObject::invokeMethod(menu,"useWindowFallbackForVerification"))return "fallback request rejected";
            qInfo()<<"Requested fallback"<<name<<menu->property("popupType");
        }
        m_controller.setFormEnabled(2,true);m_controller.setAutoCompact(false);m_controller.selectForm(0);m_controller.setVisible(true);m_controller.setBackgroundOpacity(0.6);
        m_widget->resize(440,580);m_main->hide();
    }
    if(m_stage==4){m_workspace.setThemeMode(m_cycle%2?2:1);m_workspace.setTextScale(m_cycle%2?2:1);m_controller.setLayer(m_cycle<2?1:m_cycle==2?0:2);m_widget->requestActivate();m_widget->update();}
    if(m_stage==5){
        if(m_controller.transitionPending()){if(m_wait.elapsed()<1500)return {};return "initial presentation stalled";}
        if(qAbs(m_widget->property("panelContentOpacity").toDouble()-1)>0.001)return "stable Panel content is faded";
        auto* list=find("widgetTasks");list->setProperty("currentIndex",1);
        auto* row=qobject_cast<QQuickItem*>(find("widgetTask-"+m_tasks[1]));
        if(!row)return "selected row is not presented";
        row->forceActiveFocus();
    }
    if(m_stage==6 || m_stage==9 || m_stage==12){
        m_observed=false;
        if(m_native && !scheduleSmokeMenuDismiss(m_widget,[this]{checkMenu();}))return "native cancel scheduling failed";
        if(m_stage==12){
            auto* row=qobject_cast<QQuickItem*>(find("widgetTask-"+m_tasks[0]));
            if(!row)return "keyboard row is not presented";
            row->forceActiveFocus();
            QVariant focused;QMetaObject::invokeMethod(m_widget,"focusedTaskRow",Q_RETURN_ARG(QVariant,focused));
            qInfo()<<"Keyboard task focus"<<row->hasActiveFocus()<<focused<<"blocked"<<m_widget->property("commandInputBlocked");
            QKeyEvent key(QEvent::KeyPress,Qt::Key_Menu,Qt::NoModifier);QCoreApplication::sendEvent(m_widget,&key);
        }else pointer(qobject_cast<QQuickItem*>(find("widgetTaskGesture-"+m_tasks[0])),Qt::RightButton);
    }
    if(m_stage==7 || m_stage==10 || m_stage==13){
        if(!m_native)checkMenu();
        if(!m_observed && m_error.isEmpty())m_error="native menu was never observed";
        if(!m_error.isEmpty()){QMetaObject::invokeMethod(find("widgetTaskMenu"),"close");return QStringLiteral("UX76 stage %1 cycle %2: %3").arg(m_stage).arg(m_cycle).arg(m_error);}
        if(!m_native && m_stage==7)pointer(qobject_cast<QQuickItem*>(find("widgetTaskPin")),Qt::LeftButton);
        else QMetaObject::invokeMethod(find("widgetTaskMenu"),"close");
    }
    if(m_stage==8){
        if(!m_native && (m_workspace.task(m_tasks[0]).value("pinned").toBool()!=(m_cycle%2==0) || m_workspace.task(m_tasks[1]).value("pinned").toBool()))return "menu command changed wrong task";
        auto* list=qobject_cast<QQuickItem*>(find("widgetTasks"));
        const auto at=list->mapToScene(QPointF(40,list->height()-10));
        for(const auto type:{QEvent::MouseButtonPress,QEvent::MouseButtonRelease}){
            QMouseEvent event(type,at,m_widget->mapToGlobal(at),Qt::LeftButton,type==QEvent::MouseButtonPress?Qt::LeftButton:Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(m_widget,&event);
        }
    }
    if(m_stage==11){
        if(m_main->isVisible() || !m_main->property("editId").toString().isEmpty())return "right click leaked into task details";
        // Direct Qt mouse delivery does not perform the OS activation that a
        // physical outside click would. Set up the receiving window explicitly.
        if(!m_native)m_widget->requestActivate();
    }
    if(m_stage==14){if(++m_cycle<4){m_stage=4;m_wait.restart();return {};}m_main->show();}
    ++m_stage;m_wait.restart();return m_stage==15?QStringLiteral("DONE"):QString();
}
}

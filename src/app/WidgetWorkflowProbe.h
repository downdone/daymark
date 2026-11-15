#pragma once
#include "models/TaskWorkspace.h"
#include "models/WidgetViewModel.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QMouseEvent>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QSet>

namespace daymark {
// Only constructed by the isolated smoke entry, never against a user's database.
class WidgetWorkflowProbe final {
public:
    WidgetWorkflowProbe(TaskWorkspace& workspace,WidgetViewModel& controller,QQuickWindow* main,QQuickWindow* widget,QString shot)
        :w(workspace),c(controller),main(main),widget(widget),shot(std::move(shot)){wait.start();}
    QString advance(){
        if(!w.ready() || w.busy())return {};
        auto fail=[&](const QString& why){return QStringLiteral("Widget workflow %1: %2").arg(phase).arg(why);};
        auto* inspector=find("widgetTaskInspector");auto* inbox=find("widgetReminderInbox");
        if(!inbox || (phase>=2 && !inspector))return fail("missing panels");
        if(phase==0){w.setPreference("systemReminders",false);w.setPreference("reminderPopups",false);c.selectForm(0);c.setVisible(true);w.createTodayTask(QStringLiteral("小窗流程验收"));}
        else if(phase==1){
            id=w.widgetTasks()->taskIdAt(0);if(id.isEmpty())return fail("fixture task");
            if(c.panelSidebarWidth()==0)return fail("category rail did not expand");
            width=c.panelRect().width()-c.panelSidebarWidth();
            if(!QMetaObject::invokeMethod(widget,"openSmallTask",Q_ARG(QVariant,id),Q_ARG(QVariant,QVariant::fromValue(find("widgetMore")))))return fail("open inspector");
        }else if(phase==2){
            if(!inspector->property("visible").toBool())return fail("inspector not visible");
            capturePopup(inspector,"inspector");
            auto* title=find("widgetInspectorTitle");
            if(saveCycle==0){title->setProperty("text",QString());QMetaObject::invokeMethod(inspector,"markDirty");if(!click("widgetInspectorClose"))return fail("failed save pointer");++saveCycle;return {};}
            if(inspector->property("localError").toString().isEmpty() || w.task(id).value("title").toString()!=QStringLiteral("小窗流程验收"))return fail("failed save must retain panel and original task");
            title->setProperty("text",QStringLiteral("就近修改已保存"));QMetaObject::invokeMethod(inspector,"markDirty");
            if(!click("widgetInspectorClose"))return fail("close inspector pointer");
        }else if(phase==3){
            if(inspector->property("visible").toBool() || w.task(id).value("title").toString()!=QStringLiteral("就近修改已保存"))return fail(QStringLiteral("save on close visible=%1 dirty=%2 saving=%3 error=%4 title=%5 workspace=%6").arg(inspector->property("visible").toBool()).arg(inspector->property("dirty").toBool()).arg(inspector->property("saving").toBool()).arg(inspector->property("localError").toString(),w.task(id).value("title").toString(),w.errorText()));
            if(!click("widgetReminders"))return fail("bell pointer");
        }else if(phase==4){
            if(!inbox->property("visible").toBool() || find("reminderInboxList")->property("count").toInt()!=0)return fail("empty inbox");
            QMetaObject::invokeMethod(inbox,"close");
            w.saveDetails(id,w.task(id).value("rowVersion").toLongLong(),{{"remindAtUtc",QDateTime::currentDateTimeUtc().addSecs(1).toString(Qt::ISODateWithMs)}});wait.restart();
        }else if(phase==5){
            if(w.pendingReminderCount()==0){if(wait.elapsed()<6000)return {};return fail("reminder did not arrive");}
            if(auto* automatic=main->findChild<QQuickWindow*>("reminderPopup");automatic && automatic->isVisible())return fail("disabled automatic popup");
            key=w.reminderAlerts().first().toMap().value("key").toString();click("widgetReminders");
        }else if(phase==6){
            if(find("reminderInboxList")->property("count").toInt()!=1)return fail("inbox count");
            capture(widget,"small-window");capturePopup(inbox,"reminders");
            if(!click("reminderInboxSnooze-"+key))return fail("snooze pointer");
        }else if(phase==7){
            if(w.pendingReminderCount()!=0 || !inbox->property("visible").toBool())return fail("snooze keeps panel");
            if(!click("reminderInboxUndo"))return fail("undo pointer");
        }else if(phase==8){
            if(w.pendingReminderCount()!=1)return fail("undo reminder");
            if(!click("reminderInboxClose"))return fail("close pointer");
        }else if(phase==9){
            if(w.pendingReminderCount()!=1 || inbox->property("visible").toBool())return fail("close must not acknowledge");
            QMetaObject::invokeMethod(widget,"openActivityCapture");
        }else if(phase==10){
            auto* source=find("widgetActivitySource");source->setProperty("text",w.dateOffset(3)+QStringLiteral(" 14:00—15:30，在A会议室召开复盘会。")+w.dateOffset(2)+QStringLiteral("前提交材料。"));
            if(!click("widgetActivityConfirm"))return fail("preview pointer");
        }else if(phase==11){
            if(find("widgetActivityCandidates")->property("count").toInt()!=2)return fail("two preview candidates");
            if(editCycle==0){if(!click("widgetCandidateTime-0"))return fail("edit candidate pointer");++editCycle;return {};}
            if(editCycle==1){if(!find("widgetCandidateEditor")->property("visible").toBool())return fail("candidate nearby editor");find("widgetCandidateTitle")->setProperty("text",QStringLiteral("确认后的复盘会议"));if(!click("widgetCandidateApply"))return fail("apply candidate pointer");++editCycle;return {};}
            if(find("widgetCandidateEditor")->property("visible").toBool())return fail("candidate editor did not close");capture(widget,"extraction");if(!click("widgetActivityConfirm"))return fail("confirm pointer");
        }else if(phase==12){
            if(find("widgetActivityCapture")->property("opened").toBool())return fail("create retains erroneous draft");
            w.setView("all");if(w.tasks()->rowCount()!=3 || w.widgetTasks()->rowCount()!=1)return fail("atomic create and future tasks stay out of Today");
            w.setThemeMode(2);w.setTextScale(2);
        }else if(phase==13){
            QVariant result;QMetaObject::invokeMethod(widget,"verifyLayout",Q_RETURN_ARG(QVariant,result));if(result.toString()!="PASS")return fail(result.toString());
            capture(widget,"dark-200");w.setWidgetShowShortcuts(false);
        }else if(phase==14){
            if(c.panelSidebarWidth()!=0 || c.panelRect().width()!=width)return fail("collapse must restore body width");
            return QStringLiteral("DONE");
        }
        ++phase;return {};
    }
private:
    QObject* find(const QString& name){
        QList<QObject*> queue{widget};QSet<QObject*> seen;
        while(!queue.isEmpty()){auto* value=queue.takeFirst();if(!value || seen.contains(value))continue;seen.insert(value);if(value->objectName()==name)return value;queue.append(value->children());if(auto* item=qobject_cast<QQuickItem*>(value))for(auto* child:item->childItems())queue.append(child);if(auto* window=qobject_cast<QQuickWindow*>(value))queue.append(window->contentItem());}return nullptr;
    }
    bool click(const QString& name){
        auto* item=qobject_cast<QQuickItem*>(find(name));if(!item || !item->isVisible() || !item->isEnabled() || !item->window())return false;
        qInfo()<<"workflow click"<<name<<item->size()<<item->window()->geometry()<<item->mapToScene(QPointF(item->width()/2,item->height()/2));
        const auto local=item->mapToScene(QPointF(item->width()/2,item->height()/2));auto* window=item->window();const QPointF global=window->mapToGlobal(local.toPoint());
        for(const auto type:{QEvent::MouseMove,QEvent::MouseButtonPress,QEvent::MouseButtonRelease}){
            QMouseEvent event(type,local,global,type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonPress?Qt::LeftButton:Qt::NoButton,Qt::NoModifier);
            QCoreApplication::sendEvent(window,&event);
        }
        return true;
    }
    void capture(QQuickWindow* window,const QString& name){if(!shot.isEmpty())window->grabWindow().save(shot+"."+name+".png");}
    void capturePopup(QObject* popup,const QString& name){if(auto* item=qvariant_cast<QQuickItem*>(popup->property("contentItem"));item && item->window())capture(item->window(),name);}
    TaskWorkspace& w;WidgetViewModel& c;QQuickWindow* main;QQuickWindow* widget;QString shot,id,key;int phase=0,width=0,editCycle=0,saveCycle=0;QElapsedTimer wait;
};
}

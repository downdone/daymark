#include "TimeInputSmokeProbe.h"
#include "models/TaskWorkspace.h"
#include "models/WidgetViewModel.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QCoreApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QSet>
#include <QImage>
namespace daymark {
namespace {
QObject* find(QObject* root,const QString& name) {
    QList<QObject*> queue{root};QSet<QObject*> seen;
    while(!queue.isEmpty()){auto* object=queue.takeFirst();if(!object || seen.contains(object))continue;seen.insert(object);if(object->objectName()==name)return object;queue.append(object->children());if(auto* item=qobject_cast<QQuickItem*>(object))for(auto* child:item->childItems())queue.append(child);if(auto* window=qobject_cast<QQuickWindow*>(object))queue.append(window->contentItem());}
    return nullptr;
}
void key(QQuickWindow* window,int value){QKeyEvent press(QEvent::KeyPress,value,Qt::NoModifier),release(QEvent::KeyRelease,value,Qt::NoModifier);QCoreApplication::sendEvent(window,&press);QCoreApplication::sendEvent(window,&release);}
bool invoke(QObject* object,const char* method){return object && QMetaObject::invokeMethod(object,method);}
}
TimeInputSmokeProbe::TimeInputSmokeProbe(TaskWorkspace& value,WidgetViewModel& c,QQuickWindow* m,QQuickWindow* small,QString shot):w(value),controller(c),mainWindow(m),widget(small),screenshot(std::move(shot)){}
QString TimeInputSmokeProbe::advance() {
    if(!w.ready() || w.busy())return {};
    auto* input=find(mainWindow,"newTaskTitle");auto* preview=find(mainWindow,"mainInputTimePreview");
    if(!input || !preview)return "TIME01 missing mainWindow input";
    auto fail=[&](const QString& reason){return QStringLiteral("TIME01 phase %1: %2").arg(phase).arg(reason);};
    if(phase==0){w.setView("all");mainWindow->resize(1080,760);input->setProperty("text",QStringLiteral("明天15点联系客户"));}
    else if(phase==1){
        if(!preview->property("visible").toBool() || !preview->property("summary").toString().contains("15:00"))return fail("live preview");
        if(!invoke(find(mainWindow,"mainInputignoreTime"),"clicked") || !preview->property("ignored").toBool())return fail("ignore action");
        auto* field=qobject_cast<QQuickItem*>(input);if(!field)return fail("input item");field->forceActiveFocus();
        key(mainWindow,Qt::Key_Tab);if(field->hasActiveFocus())return fail("Tab did not move focus");
        field->forceActiveFocus();key(mainWindow,Qt::Key_Return);
    }else if(phase==2){
        if(w.tasks()->items().size()!=1 || !w.tasks()->items().first().remindAtUtc.isEmpty() || !input->property("text").toString().isEmpty() || preview->property("ignored").toBool())return fail("ignore persisted or reset");
        input->setProperty("text",QStringLiteral("明天15点再次联系客户"));if(!invoke(preview,"openEditor"))return fail("open time editor");
    }else if(phase==3){
        if(popupCycle==0){if(!preview->property("popupOpen").toBool())return fail("time popup");key(mainWindow,Qt::Key_Escape);popupCycle=1;return {};}
        if(popupCycle==1){if(preview->property("popupOpen").toBool())return fail("Escape did not close picker");if(input->property("text").toString()!=QStringLiteral("明天15点再次联系客户"))return fail("Escape changed title");invoke(preview,"openEditor");popupCycle=2;return {};}
        if(!preview->property("popupOpen").toBool())return fail("reopen after cancel");
        auto* date=find(mainWindow,"mainInputtimeDate");auto* time=find(mainWindow,"mainInputtimeClock");auto* remind=find(mainWindow,"mainInputtimeRemind");
        if(!date || !time || !remind)return fail("manual fields");
        if(!screenshot.isEmpty() && !mainWindow->grabWindow().save(screenshot+".time-picker.png"))return fail("picker capture");
        date->setProperty("text",w.dateOffset(2));time->setProperty("text","09:30");remind->setProperty("checked",true);
        if(!invoke(find(mainWindow,"mainInputapplyTime"),"clicked"))return fail("manual apply");
    }else if(phase==4){if(!preview->property("manual").toBool() || !invoke(input,"accepted"))return fail("manual save");}
    else if(phase==5){
        if(w.tasks()->items().size()!=2 || !input->property("text").toString().isEmpty() || preview->property("manual").toBool())return fail("manual reset");
        bool found=false;for(const auto& task:w.tasks()->items())if(task.title==QStringLiteral("明天15点再次联系客户")){found=true;if(!task.startOn || task.startOn->toString(Qt::ISODate)!=w.dateOffset(2) || QDateTime::fromString(task.remindAtUtc,Qt::ISODateWithMs).toLocalTime().time()!=QTime(9,30))return fail("manual fields not persisted");}if(!found)return fail("manual task missing");
        input->setProperty("text",QStringLiteral("明天15点输入法测试"));if(auto* item=qobject_cast<QQuickItem*>(input))item->forceActiveFocus();QInputMethodEvent composing("shi",{});QCoreApplication::sendEvent(input,&composing);before=int(w.tasks()->items().size());
        if(!input->property("inputMethodComposing").toBool() || !invoke(input,"accepted"))return fail("IME setup");
    }else if(phase==6){
        if(w.tasks()->items().size()!=before)return fail("IME committed a task");
        QInputMethodEvent clear;QCoreApplication::sendEvent(input,&clear);
        preview->setProperty("overrides",QVariantMap{{"date",w.dateOffset(1)},{"time","25:00"},{"target","planned"},{"reminder",true}});invoke(input,"accepted");
    }else if(phase==7){
        if(w.errorText().isEmpty() || input->property("text").toString().isEmpty() || !preview->property("manual").toBool() || w.tasks()->items().size()!=before)return fail("failed input did not preserve draft");
        w.clearError();input->setProperty("text",QString());controller.setMini(false);controller.setVisible(true);invoke(find(widget,"widgetAdd"),"clicked");
    }else if(phase==8){
        auto* small=find(widget,"widgetQuickTitle");auto* suggestion=find(widget,"widgetInputTimePreview");if(!small || !suggestion)return fail("widget input");small->setProperty("text",QStringLiteral("明天15点小窗录入"));
    }else if(phase==9){
        auto* suggestion=find(widget,"widgetInputTimePreview");if(!suggestion || !suggestion->property("visible").toBool())return fail("widget preview");
        if(!screenshot.isEmpty() && !widget->grabWindow().save(screenshot+".time-widget.png"))return fail("widget capture");
        if(!invoke(widget,"submitTask"))return fail("widget submit");
    }else if(phase==10){
        bool found=false;for(const auto& task:w.tasks()->items())if(task.title==QStringLiteral("明天15点小窗录入")){found=true;if(!task.startOn || task.startOn->toString(Qt::ISODate)!=w.dateOffset(1) || task.remindAtUtc.isEmpty())return fail("widget date overrides today");}if(!found)return fail("widget task missing");
        if(!find(widget,"widgetQuickTitle")->property("text").toString().isEmpty())return fail("widget draft reset");
        input->setProperty("text",QStringLiteral("明天15点浅色预览"));if(!screenshot.isEmpty() && !mainWindow->grabWindow().save(screenshot+".time-light.png"))return fail("light capture");
        w.setThemeMode(2);w.setTextScale(2);mainWindow->resize(850,700);input->setProperty("text",QStringLiteral("明天15点深色大字预览"));
    }else if(phase==11){
        auto* suggestion=qobject_cast<QQuickItem*>(find(mainWindow,"mainInputtimeSuggestion"));if(!suggestion || suggestion->width()<=0 || suggestion->height()<=0)return fail("scaled suggestion geometry");
        if(!screenshot.isEmpty() && !mainWindow->grabWindow().save(screenshot+".time-dark-200.png"))return fail("dark capture");
        return "DONE";
    }
    ++phase;return {};
}
}

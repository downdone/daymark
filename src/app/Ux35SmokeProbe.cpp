#include "Ux35SmokeProbe.h"
#include "models/TaskWorkspace.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QSet>
namespace daymark {
Ux35SmokeProbe::Ux35SmokeProbe(TaskWorkspace& workspace,QQuickWindow* window,QString screenshot):m_workspace(workspace),m_window(window),m_screenshot(std::move(screenshot)){m_wait.start();m_inputClock.start();}
QObject* Ux35SmokeProbe::find(const QString& name)const
{
    QList<QObject*> pending{m_window};QSet<QObject*> visited;
    while(!pending.isEmpty()){
        auto* item=pending.takeLast();if(!item || visited.contains(item))continue;visited.insert(item);
        if(item->objectName()==name){auto* visual=qobject_cast<QQuickItem*>(item);if(!visual || visual->window()==m_window)return item;}
        for(auto* child:item->children())pending.append(child);
        if(auto* visual=qobject_cast<QQuickItem*>(item))for(auto* child:visual->childItems())pending.append(child);
        if(auto* window=qobject_cast<QQuickWindow*>(item))pending.append(window->contentItem());
    }
    return nullptr;
}
bool Ux35SmokeProbe::invoke(QObject* object,const char* method,const QVariant& value)
{return object && (value.isValid()?QMetaObject::invokeMethod(object,method,Q_ARG(QVariant,value)):QMetaObject::invokeMethod(object,method));}
bool Ux35SmokeProbe::capture(const QString& suffix)const
{return m_screenshot.isEmpty() || m_window->grabWindow().save(m_screenshot+QStringLiteral(".ux35-")+suffix+QStringLiteral(".png"));}
void Ux35SmokeProbe::click(const QPointF& at)
{
    for(const auto type:{QEvent::MouseButtonPress,QEvent::MouseButtonRelease}){
        QMouseEvent event(type,at,m_window->mapToGlobal(at),Qt::LeftButton,type==QEvent::MouseButtonPress?Qt::LeftButton:Qt::NoButton,Qt::NoModifier);
        event.setTimestamp(m_inputClock.elapsed()+m_inputSequence++);QCoreApplication::sendEvent(m_window,&event);
    }
}
QString Ux35SmokeProbe::advance()
{
    if(!m_window)return QStringLiteral("UX35 window destroyed");if(!m_workspace.ready() || m_workspace.busy())return {};
    auto fail=[this](const QString& value){return QStringLiteral("UX35 stage %1: ").arg(m_stage)+value;};
    auto settled=[&]{m_window->grabWindow();return m_wait.elapsed()>160;};
    auto* panel=find("calendarPanel");auto* editor=find("taskEditor");
    if(!panel || !editor)return fail("workspace controls");
    if(m_stage==0){
        if(QGuiApplication::platformName()!=QStringLiteral("offscreen") && !m_window->isActive()){m_window->show();m_window->requestActivate();return {};}
        m_workspace.setThemeMode(1);m_workspace.setTextScale(1);m_window->resize(1280,800);
        if(!m_workspace.saveCatalog({{"kind","project"},{"title",QStringLiteral("连续月历验收")}}))return fail("category fixture");
    }else if(m_stage==1){
        for(const auto& value:m_workspace.categoryTree())if(value.toMap().value("title")==QStringLiteral("连续月历验收"))m_scope="project:"+value.toMap().value("id").toString();
        if(m_scope.isEmpty())return fail("scope identity");m_workspace.setView(m_scope);
        if(!m_workspace.createForDate(QStringLiteral("跨月安排"),"2026-10-30"))return fail("task fixture");
    }else if(m_stage==2){
        const auto task=m_workspace.tasks()->items().first();m_task=task.id;
        if(!m_workspace.saveTaskWithDetails(task.id,task.rowVersion,task.title,task.notes,"ACTIVE","2026-10-30","2026-11-04",false,{}))return fail("cross month fixture");
    }else if(m_stage==3){m_before=m_workspace.task(m_task);m_workspace.selectCalendarDate("2026-10-25");m_workspace.setView("month");}
    else if(m_stage==4){
        if(!settled())return {};auto* stream=find("calendarMonthGrid");auto* list=qobject_cast<QQuickItem*>(find("continuousMonthList"));
        if(!stream || !list || stream->property("weeks").toList().size()>20 || list->height()<250)return fail("virtualized week surface");
        if(!capture("month-top-and-icon-footer"))return fail("month capture");
        const auto at=list->mapToScene(QPointF(list->width()/2,list->height()/2));
        QWheelEvent wheel(at,m_window->mapToGlobal(at),QPoint(0,-700),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        wheel.setTimestamp(m_inputClock.elapsed()+m_inputSequence++);QCoreApplication::sendEvent(m_window,&wheel);
    }else if(m_stage==5){
        auto* stream=find("calendarMonthGrid");auto* list=find("continuousMonthList");
        if(stream->property("visibleMonth").toString()<"2026-11-01" && m_wait.elapsed()<1600){auto* visual=qobject_cast<QQuickItem*>(list);const auto at=visual->mapToScene(QPointF(visual->width()/2,visual->height()/2));QWheelEvent wheel(at,m_window->mapToGlobal(at),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);wheel.setTimestamp(m_inputClock.elapsed()+m_inputSequence++);QCoreApplication::sendEvent(m_window,&wheel);return {};}
        if(m_wait.elapsed()<400)return {};
        if(list->property("contentY").toDouble()<=0 || stream->property("visibleMonth").toString()<"2026-11-01" || m_workspace.task(m_task)!=m_before || !capture("month-scroll-across-boundary"))return fail("wheel scrolls to next month without changing task");
        const auto weeks=stream->property("weeks").toList();m_windowAnchor=stream->property("windowAnchor").toString();const int index=weeks.size()-2;QMetaObject::invokeMethod(list,"positionViewAtIndex",Q_ARG(int,index),Q_ARG(int,0));
    }else if(m_stage==6){
        if(m_wait.elapsed()<400)return {};auto* stream=find("calendarMonthGrid");
        if(stream->property("windowAnchor").toString()<=m_windowAnchor || stream->property("weeks").toList().size()>20 || m_workspace.task(m_task)!=m_before)return fail("bounded forward window recycles");
        if(!capture("month-recycled"))return fail("recycled capture");auto* list=find("continuousMonthList");QMetaObject::invokeMethod(list,"positionViewAtIndex",Q_ARG(int,1),Q_ARG(int,0));
    }else if(m_stage==7){if(m_wait.elapsed()<400)return {};auto* stream=find("calendarMonthGrid");if(stream->property("windowAnchor").toString()!=m_windowAnchor || m_workspace.task(m_task)!=m_before)return fail("reverse recycling keeps task unchanged");m_workspace.selectCalendarDate("2026-10-25");m_workspace.setView("day");
    }else if(m_stage==8){
        if(!settled())return {};auto* day=qobject_cast<QQuickItem*>(find("calendarTimelineDay-2026-10-25"));if(!day)return fail("day surface");click(day->mapToScene(QPointF(day->width()/2,9*60*0.8)));
    }else if(m_stage==9){
        auto* popup=find("calendarCreatePopup");
        if(!popup || !popup->property("visible").toBool() || panel->property("newMinute").toInt()!=540 || panel->property("newDuration").toInt()!=60 || !capture("day-click-create"))return fail("single click opens hour creation");
        invoke(popup,"finish");if(popup->property("visible").toBool() || m_workspace.task(m_task)!=m_before || m_workspace.timelineFor("2026-10-25",false).value("blocks").toList().size())return fail("empty draft finish creates nothing");
        auto* day=qobject_cast<QQuickItem*>(find("calendarTimelineDay-2026-10-25"));click(day->mapToScene(QPointF(day->width()/2,10*60*0.8)));
    }else if(m_stage==10){auto* title=find("calendarCreateTitle");title->setProperty("text",QStringLiteral("单击时刻创建"));find("calendarCreateNotes")->setProperty("text",QStringLiteral("新增面板中的备注"));find("calendarCreateEnd")->setProperty("text",QStringLiteral("2026-10-24"));invoke(title,"accepted");}
    else if(m_stage==11){
        if(!m_retriedDraft){
            auto* popup=find("calendarCreatePopup");if(m_workspace.errorText().isEmpty() || !popup->property("visible").toBool() || find("calendarCreateNotes")->property("text").toString()!=QStringLiteral("新增面板中的备注") || m_workspace.timelineFor("2026-10-25",false).value("blocks").toList().size() || !capture("invalid-draft-retained"))return fail("invalid new draft remains without insert");
            find("calendarCreateEnd")->setProperty("text",QStringLiteral("2026-10-25"));m_retriedDraft=true;invoke(find("calendarCreateTitle"),"accepted");m_wait.restart();return {};
        }
        if(!settled())return {};const auto blocks=m_workspace.timelineFor("2026-10-25",false).value("blocks").toList();
        if(blocks.size()!=1 || blocks.first().toMap().value("schedule").toMap().value("startTime")!="10:00" || blocks.first().toMap().value("schedule").toMap().value("endTime")!="11:00" || editor->property("visible").toBool() || find("calendarCreatePopup")->property("visible").toBool())return fail("hour create same scope and return to calendar");
        if(m_workspace.task(blocks.first().toMap().value("taskId").toString()).value("notes").toString()!=QStringLiteral("新增面板中的备注"))return fail("new draft metadata persisted atomically");
        const auto createdId=blocks.first().toMap().value("taskId").toString();
        if(!QMetaObject::invokeMethod(panel,"openTask",Q_ARG(QString,createdId)) || !editor->property("visible").toBool() || m_window->property("editId").toString()!=createdId)return fail("created task opens same identity");invoke(editor,"close");m_workspace.setView(m_scope);
    }else if(m_stage==12){
        if(!settled())return {};auto* settings=qobject_cast<QQuickItem*>(find("settingsAndTools"));auto* backup=qobject_cast<QQuickItem*>(find("dataAndBackup"));
        if(!settings || !backup || settings->width()<32 || backup->width()<32 || settings->property("description").toString().isEmpty() || backup->property("description").toString().isEmpty() || settings->property("text").toString().size() || backup->property("text").toString().size() || !capture("compact-navigation"))return fail("footer icon names and hit areas");
        m_workspace.setView("month");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_window->resize(880,580);
    }else if(m_stage==13){if(!settled())return {};if(!capture("month-dark-200"))return fail("large month");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_window->resize(1280,800);m_workspace.setView("month");}
    else if(m_stage==14){if(!settled())return {};if(!capture("month-final"))return fail("final month");panel->setProperty("selectedDate",QStringLiteral("2026-11-03"));invoke(panel,"setScale",QStringLiteral("day"));}
    else if(m_stage==15){if(!settled())return {};if(m_workspace.calendarDate()!=QStringLiteral("2026-11-03") || m_workspace.calendarScale()!=QStringLiteral("day") || m_workspace.task(m_task)!=m_before || !capture("selected-day-retained"))return fail("month selection survives scale switch");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_window->resize(880,580);
        if(!QMetaObject::invokeMethod(panel,"beginCreate",Q_ARG(QVariant,QVariant(QStringLiteral("2026-11-03"))),Q_ARG(QVariant,QVariant(540)),Q_ARG(QVariant,QVariant(60)),Q_ARG(QVariant,QVariant(QPointF(450,80)))))return fail("large draft open");}
    else if(m_stage==16){if(!settled())return {};auto* popup=find("calendarCreatePopup");auto* scroller=qobject_cast<QQuickItem*>(find("calendarCreateScroll"));if(!popup->property("visible").toBool() || popup->property("height").toDouble()>m_window->height()-16 || !scroller || scroller->height()<80 || !capture("new-draft-dark-200"))return fail("large draft scroll remains reachable");find("calendarCreateTitle")->setProperty("text",QStringLiteral("退出新增即保存"));find("calendarCreateNotes")->setProperty("text",QStringLiteral("结束草稿后保持日期"));click(QPointF(4,m_window->height()/2));}
    else if(m_stage==17){if(!settled())return {};const auto blocks=m_workspace.timelineFor("2026-11-03",false).value("blocks").toList();if(find("calendarCreatePopup")->property("visible").toBool() || blocks.size()!=1 || editor->property("visible").toBool() || m_workspace.task(blocks.first().toMap().value("taskId").toString()).value("notes").toString()!=QStringLiteral("结束草稿后保持日期"))return fail("finish saves whole draft and closes");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_window->resize(1280,800);}
    else if(m_stage==18){if(!settled())return {};if(!capture("compact-day-final"))return fail("compact final");return QStringLiteral("DONE");}
    qInfo()<<"UX35 scenario"<<m_stage<<"complete";++m_stage;m_wait.restart();return {};
}
}

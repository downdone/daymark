#include "Ux37SmokeProbe.h"
#include "models/TaskWorkspace.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QColor>
#include <QSet>
#include <QDebug>
namespace daymark {
Ux37SmokeProbe::Ux37SmokeProbe(TaskWorkspace& workspace,QQuickWindow* window,QString screenshot):m_workspace(workspace),m_window(window),m_screenshot(std::move(screenshot)){m_wait.start();m_clock.start();
    QObject::connect(window,&QQuickWindow::frameSwapped,window,[this]{
        if(!m_awaitingPanelFrame)return;
        auto* editor=find("taskEditor");
        if(editor && editor->property("visible").toBool() && m_window->property("editId")==m_parent){m_panelFrameMs=m_presentClock.elapsed();m_awaitingPanelFrame=false;}
    });
}
QObject* Ux37SmokeProbe::find(const QString& name) const {
    QList<QObject*> pending{m_window};QSet<QObject*> seen;
    while(!pending.isEmpty()){
        auto* object=pending.takeLast();if(!object || seen.contains(object))continue;seen.insert(object);
        if(object->objectName()==name)return object;
        for(auto* child:object->children())pending.append(child);
        if(auto* item=qobject_cast<QQuickItem*>(object))for(auto* child:item->childItems())pending.append(child);
        if(auto* window=qobject_cast<QQuickWindow*>(object))pending.append(window->contentItem());
    }
    return nullptr;
}
void Ux37SmokeProbe::click(QQuickItem* item,Qt::KeyboardModifiers modifiers,Qt::MouseButton button) {
    if(!item)return;
    const auto at=item->mapToScene(QPointF(item->width()*0.65,item->height()/2));
    for(const auto type:{QEvent::MouseMove,QEvent::MouseButtonPress,QEvent::MouseButtonRelease}){
        QMouseEvent event(type,at,m_window->mapToGlobal(at),type==QEvent::MouseMove?Qt::NoButton:button,type==QEvent::MouseButtonRelease?Qt::NoButton:button,modifiers);
        event.setTimestamp(m_clock.elapsed()+m_sequence++);QCoreApplication::sendEvent(m_window,&event);
    }
}
void Ux37SmokeProbe::hover(QQuickItem* item) {
    if(!item)return;
    const auto at=item->mapToScene(QPointF(item->width()/2,item->height()/2));
    QMouseEvent event(QEvent::MouseMove,at,m_window->mapToGlobal(at),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
    event.setTimestamp(m_clock.elapsed()+m_sequence++);QCoreApplication::sendEvent(m_window,&event);
}
void Ux37SmokeProbe::key(QQuickItem* target,int code,Qt::KeyboardModifiers modifiers) {
    for(const auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent event(type,code,modifiers);QCoreApplication::sendEvent(target,&event);}
}
bool Ux37SmokeProbe::capture(const QString& name) const {return m_screenshot.isEmpty() || m_window->grabWindow().save(m_screenshot+".ux37-"+name+".png");}
QString Ux37SmokeProbe::advance() {
    if(m_workspace.busy())return {};
    if(m_wait.elapsed()<140)return {};
    m_window->grabWindow();
    auto fail=[this](const QString& reason){return QStringLiteral("UX37 stage %1: %2").arg(m_stage).arg(reason);};
    auto* selection=find("taskSelection");auto* list=qobject_cast<QQuickItem*>(find("taskList"));
    if(!selection || !list)return fail("selection controls missing");
    auto row=[&](int index){return qobject_cast<QQuickItem*>(find("taskRow-"+m_ids.value(index)));};
    auto count=[&]{return selection->property("count").toInt();};
    auto invoke=[&](QObject* object,const char* method){return object && QMetaObject::invokeMethod(object,method);};
    auto menu=[&]{return find("batchTaskContext");};
    auto activeFill=[&](QObject* item){auto* surface=item?item->findChild<QQuickItem*>("menuItemSurface"):nullptr;return surface && surface->property("color").value<QColor>().alpha()>0;};
    if(m_stage==0){m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_window->resize(1280,800);m_workspace.setView("inbox");if(!m_workspace.createTask("父任务：交互验收"))return fail("create parent");}
    else if(m_stage==1){m_parent=m_workspace.tasks()->items().first().id;if(!m_workspace.createSubtask("子任务：区间选择",m_parent))return fail("create child");}
    else if(m_stage>=2 && m_stage<=4){if(!m_workspace.createTask(QStringLiteral("独立任务 %1").arg(m_stage)))return fail("create task");}
    else if(m_stage==5){for(const auto& task:m_workspace.tasks()->items())m_ids.append(task.id);if(m_ids.size()!=5 || !row(0) || !row(4))return fail("visible fixture");if(row(0)->property("relationSymbol")!="subtasks" || row(1)->property("relationSymbol")!="subtask" || row(2)->property("relationSymbol")!="")return fail("stable parent and child icons");click(row(0));}
    else if(m_stage==6){if(count()!=1 || selection->property("anchorId")!=m_ids[0])return fail("plain click anchor");click(row(2),Qt::ShiftModifier);}
    else if(m_stage==7){if(count()!=3 || !m_window->property("batchMode").toBool() || find("taskQuickEditor")->property("editing").toBool())return fail("Shift includes visible child and closes editing");click(row(1),Qt::ControlModifier);}
    else if(m_stage==8){if(count()!=2)return fail("Command/Ctrl toggles only target");click(row(2),Qt::NoModifier,Qt::RightButton);}
    else if(m_stage==9){if(count()!=2 || !menu()->property("visible").toBool())return fail("right click preserves selected collection");hover(qobject_cast<QQuickItem*>(find("batchMenuComplete")));}
    else if(m_stage==10){hover(qobject_cast<QQuickItem*>(find("batchMenuSummary")));}
    else if(m_stage==11){if(activeFill(find("batchMenuSummary")))return fail("disabled menu header painted active");if(!capture("multi-context-light"))return fail("capture");invoke(menu(),"close");click(row(4),Qt::NoModifier,Qt::RightButton);}
    else if(m_stage==12){if(count()!=1 || menu()->property("visible").toBool() || !find("taskContext-"+m_ids[4])->property("visible").toBool())return fail("outside selection right click becomes single context");invoke(find("taskContext-"+m_ids[4]),"close");list->forceActiveFocus();key(list,Qt::Key_A,Qt::ControlModifier);}
    else if(m_stage==13){if(count()!=5)return fail("list select all");key(list,Qt::Key_Escape);}
    else if(m_stage==14){if(m_window->property("batchMode").toBool() || count()!=0)return fail("Escape clears selection");click(row(0));}
    else if(m_stage==15){auto* title=qobject_cast<QQuickItem*>(find("quickEditorTitle"));if(!title)return fail("title editor");key(title,Qt::Key_A,Qt::ControlModifier);if(title->property("selectedText")!=title->property("text") || count()!=1 || m_window->property("batchMode").toBool())return fail("text select all stays in text");invoke(find("taskQuickEditor"),"closeRequested");}
    else if(m_stage==16){list->forceActiveFocus();key(list,Qt::Key_Down,Qt::ShiftModifier);}
    else if(m_stage==17){if(count()!=2)return fail("keyboard range extension");if(!m_workspace.batchTasks(m_window->property("batchSelection").toList(),"complete",{}))return fail("complete selection");}
    else if(m_stage==18){QMetaObject::invokeMethod(m_window,"changeView",Q_ARG(QVariant,QVariant("completed")));}
    else if(m_stage==19){m_ids.clear();for(const auto& task:m_workspace.tasks()->items())m_ids.append(task.id);if(m_ids.size()!=2)return fail("completed fixture");click(row(0),Qt::NoModifier,Qt::RightButton);}
    else if(m_stage==20){hover(qobject_cast<QQuickItem*>(find("taskMenuState-"+m_ids[0])));}
    else if(m_stage==21){QVariant point;QMetaObject::invokeMethod(row(0),"hoverDisabledMenuPosition",Q_RETURN_ARG(QVariant,point));const auto map=point.toMap();if(map.isEmpty())return fail("disabled move entry");const QPointF at(map.value("x").toReal(),map.value("y").toReal());QMouseEvent event(QEvent::MouseMove,at,m_window->mapToGlobal(at),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(m_window,&event);}
    else if(m_stage==22){QVariant result;QMetaObject::invokeMethod(row(0),"verifyMenuHighlight",Q_RETURN_ARG(QVariant,result));if(result!="PASS" || !capture("single-disabled-hover"))return fail("completed menu exclusive highlight: "+result.toString());invoke(find("taskContext-"+m_ids[0]),"close");click(row(0));}
    else if(m_stage==23){click(row(1),Qt::ShiftModifier);}
    else if(m_stage==24){if(count()!=2)return fail("completed Shift selection");click(row(0),Qt::NoModifier,Qt::RightButton);}
    else if(m_stage==25){if(!menu()->property("visible").toBool() || !find("batchMenuReopen")->property("visible").toBool() || find("batchMenuComplete")->property("visible").toBool())return fail("completed multi context capabilities");if(!capture("completed-multi-context"))return fail("capture completed");click(qobject_cast<QQuickItem*>(find("batchMenuReopen")));}
    else if(m_stage==26){auto* dialog=find("batchTaskDialog");if(!dialog->property("visible").toBool() || dialog->property("initialAction").toInt()!=4)return fail("reopen preview");click(qobject_cast<QQuickItem*>(find("applyBatchSelection")));}
    else if(m_stage==27){if(m_workspace.tasks()->rowCount()!=0 || count()!=0 || find("batchTaskDialog")->property("visible").toBool())return fail("reopen atomic completion and selection cleanup");QMetaObject::invokeMethod(m_window,"changeView",Q_ARG(QVariant,QVariant("inbox")));}
    else if(m_stage==28){m_ids.clear();for(const auto& task:m_workspace.tasks()->items())m_ids.append(task.id);click(row(0));}
    else if(m_stage==29){click(row(3),Qt::ShiftModifier);}
    else if(m_stage==30){m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_window->resize(880,580);}
    else if(m_stage==31){if(count()!=4 || !capture("selection-dark-200"))return fail("selection survives density and theme");m_workspace.setSearch("no matching synthetic task");}
    else if(m_stage==32){if(m_wait.elapsed()<350)return {};if(m_workspace.tasks()->rowCount()!=0 || count()!=0){if(m_wait.elapsed()<1500)return {};capture("failed-filtered-selection");return fail(QStringLiteral("selection drops filtered-out identities: rows=%1 selected=%2").arg(m_workspace.tasks()->rowCount()).arg(count()));}m_workspace.setSearch("");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_window->resize(1280,800);}
    else if(m_stage==33){if(m_wait.elapsed()<350)return {};list->forceActiveFocus();key(list,Qt::Key_Escape);if(!m_workspace.createForTime(QStringLiteral("紧凑属性行验收"),m_workspace.dateOffset(0),540))return fail("timed density fixture");}
    else if(m_stage==34){for(const auto& task:m_workspace.tasks()->items())if(task.title==QStringLiteral("紧凑属性行验收"))m_parent=task.id;auto* details=qobject_cast<QQuickItem*>(find("taskDetails-"+m_parent));if(!details)return fail("detail entry");m_panelFrameMs=-1;m_presentClock.start();m_awaitingPanelFrame=true;click(details);}
    else if(m_stage==35){auto* editor=qobject_cast<QQuickItem*>(find("taskEditor"));auto* title=qobject_cast<QQuickItem*>(find("editTaskTitle"));auto* summary=qobject_cast<QQuickItem*>(find("editorTimeSummary"));if(!editor || !editor->isVisible() || !summary || !summary->isVisible() || title->mapToItem(editor,QPointF()).y()>64 || editor->height()>440){capture("failed-compact");return fail(QStringLiteral("compact header and content height: visible=%1 summary=%2 titleY=%3 height=%4 expanded=%5").arg(editor && editor->isVisible()).arg(summary && summary->isVisible()).arg(title && editor ? title->mapToItem(editor,QPointF()).y() : -1).arg(editor ? editor->height() : -1).arg(m_window->property("inspectorExpanded").toBool()));}qInfo()<<"UX37 detail first swapped frame ms="<<m_panelFrameMs<<"platform="<<QGuiApplication::platformName();if(!capture("compact-inspector"))return fail("compact capture");click(summary);}
    else if(m_stage==36){auto* date=qobject_cast<QQuickItem*>(find("editStartDate"));auto* time=qobject_cast<QQuickItem*>(find("taskTimeStart"));if(!date || !time || !date->isVisible() || !time->isVisible() || qAbs(date->mapToScene(QPointF()).y()-time->mapToScene(QPointF()).y())>1 || date->mapToScene(QPointF()).x()+date->width()>time->mapToScene(QPointF()).x()+1)return fail("start date and time share a non-overlapping row");if(!capture("compact-time-expanded"))return fail("time capture");m_workspace.setPreference("compact",false);}
    else if(m_stage==37){if(!capture("comfortable-inspector"))return fail("comfortable capture");m_workspace.setPreference("compact",true);m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_window->resize(880,580);}
    else if(m_stage==38){auto* scroll=qobject_cast<QQuickItem*>(find("taskEditorScroll"));auto* title=qobject_cast<QQuickItem*>(find("editTaskTitle"));title->forceActiveFocus();if(!scroll || scroll->width()<200 || scroll->height()<96 || !capture("inspector-dark-200"))return fail("readable expanded inspector");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_window->resize(1280,800);}
    else if(m_stage==39){auto* choice=qobject_cast<QQuickItem*>(find("taskRepeatChoice"));if(!choice || !choice->isVisible())return fail("repeat property entry");click(choice);key(choice,Qt::Key_End);key(choice,Qt::Key_Return);}
    else if(m_stage==40){auto* frequency=qobject_cast<QQuickItem*>(find("taskRepeatFrequency"));if(!find("taskTimingFields")->property("repeatExpanded").toBool() || !frequency || !frequency->isVisible())return fail("custom repeat expands in the same inspector");if(!capture("custom-repeat"))return fail("repeat capture");auto* choice=qobject_cast<QQuickItem*>(find("taskRepeatChoice"));click(choice);key(choice,Qt::Key_Home);key(choice,Qt::Key_Down);key(choice,Qt::Key_Down);key(choice,Qt::Key_Return);}
    else if(m_stage==41){QVariant patch;QMetaObject::invokeMethod(find("taskTimingFields"),"detailsPatch",Q_RETURN_ARG(QVariant,patch));if(patch.toMap().value("repeat").toMap().value("frequency")!="WEEKLY")return fail("repeat preset draft");invoke(m_window,"saveEditor");}
    else if(m_stage==42){if(m_workspace.task(m_parent).value("repeat").toMap().value("frequency")!="WEEKLY" || !m_workspace.errorText().isEmpty())return fail("repeat preset persists with the same task");invoke(find("taskEditor"),"close");return "DONE";}
    qInfo()<<"UX37 scenario"<<m_stage<<"complete";++m_stage;m_wait.restart();return {};
}
}

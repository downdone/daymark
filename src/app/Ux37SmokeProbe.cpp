#include "Ux37SmokeProbe.h"
#include "models/TaskWorkspace.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QColor>
#include <QJSValue>
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
    // These controls live for the main window's lifetime. Delegates and other
    // dynamic items still use a fresh traversal; never cache task identities.
    const bool stable=name=="taskSelection" || name=="taskList" || name=="batchTaskContext" || name=="trashTaskContext" || name=="categoryContextMenu" || name=="taskEditor";
    if(stable){const auto cached=m_stableControls.value(name);if(cached)return cached;}
    QList<QObject*> pending{m_window};QSet<QObject*> seen;
    while(!pending.isEmpty()){
        auto* object=pending.takeLast();if(!object || seen.contains(object))continue;seen.insert(object);
        if(object->objectName()==name){if(stable)m_stableControls.insert(name,object);return object;}
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
    auto fail=[this](const QString& reason){capture("failure");return QStringLiteral("UX37 stage %1: %2").arg(m_stage).arg(reason);};
    auto* selection=find("taskSelection");auto* list=qobject_cast<QQuickItem*>(find("taskList"));
    if(!selection || !list)return fail("selection controls missing");
    auto row=[&](int index){QVariant result;const auto id=m_ids.value(index);QMetaObject::invokeMethod(m_window,"taskRowForVerification",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,QVariant(id)));auto* item=qobject_cast<QQuickItem*>(result.value<QObject*>());return item && item->property("taskId")==id ? item : nullptr;};
    auto count=[&]{return selection->property("count").toInt();};
    auto invoke=[&](QObject* object,const char* method){return object && QMetaObject::invokeMethod(object,method);};
    auto menu=[&]{return find("batchTaskContext");};
    for(const auto& id:{QStringLiteral("batchTaskContext"),QStringLiteral("trashTaskContext"),QStringLiteral("categoryContextMenu")}){auto* context=find(id);if(context && context->property("visible").toBool()){QVariant result;QMetaObject::invokeMethod(context,"verifyGrouping",Q_RETURN_ARG(QVariant,result));if(result!="PASS")return fail("visible menu group boundaries: "+id+" / "+result.toString());}}

    auto activeFill=[&](QObject* item){auto* surface=item?item->findChild<QQuickItem*>("menuItemSurface"):nullptr;return surface && surface->property("color").value<QColor>().alpha()>0;};
    auto columns=[&](QQuickItem* item){QVariant result;if(item)QMetaObject::invokeMethod(item,"columnPositions",Q_RETURN_ARG(QVariant,result));return result.toMap();};
    auto draftTags=[&]{const auto value=find("taskDetailsFields")->property("tagIds");return value.metaType()==QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant().toStringList() : value.toStringList();};
    auto windowKey=[&](int code,Qt::KeyboardModifiers modifiers=Qt::NoModifier){for(const auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent event(type,code,modifiers);QCoreApplication::sendEvent(m_window,&event);}};
    auto pointer=[&](QEvent::Type type,const QPointF& at){QMouseEvent event(type,at,m_window->mapToGlobal(at),type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);event.setTimestamp(m_clock.elapsed()+m_sequence++);QCoreApplication::sendEvent(m_window,&event);};
    if(m_stage<=60){
    if(m_stage==0){m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_window->resize(1280,800);m_workspace.setView("inbox");if(!m_workspace.createTask("父任务：交互验收"))return fail("create parent");}
    if(m_stage==1){m_parent=m_workspace.tasks()->items().first().id;if(!m_workspace.createSubtask("子任务：区间选择",m_parent))return fail("create child");}
    if(m_stage>=2 && m_stage<=4){if(!m_workspace.createTask(QStringLiteral("独立任务 %1").arg(m_stage)))return fail("create task");}
    if(m_stage==5){
        if(m_firstInspectorPhase==0){for(const auto& task:m_workspace.tasks()->items())m_ids.append(task.id);if(m_ids.size()!=5 || !row(0) || !row(4))return fail("visible fixture");if(row(0)->property("relationSymbol")!="subtasks" || row(1)->property("relationSymbol")!="subtask" || row(2)->property("relationSymbol")!="")return fail("stable parent and child icons");invoke(row(0),"subtaskRequested");++m_firstInspectorPhase;m_wait.restart();return {};}
        auto* editor=qobject_cast<QQuickItem*>(find("taskEditor"));auto* surface=find("taskEditorSurface");
        const auto point=surface->property("sourcePoint").toPointF();
        auto* title=qobject_cast<QQuickItem*>(find("taskTitle-"+m_ids[0]));const auto inset=title->property("topPadding").toReal();const auto titleY=title->mapToScene(QPointF(0,inset+(title->height()-inset)/2)).y();
        if(!editor->isVisible() || editor->x()<=m_window->property("sidebarWidth").toReal()+12 || surface->property("arrowEdge")!="left" || qAbs(point.y()+editor->y()-titleY)>1 || qAbs(surface->property("arrowOffset").toReal()-point.y())>1)return fail("first subtask entry positions beside title with accurate arrow");
        if(m_firstInspectorPhase==1){if(m_window->property("editorTab").toInt()!=1 || !capture("first-open-source"))return fail("first subtask tab");m_columns["sourcePoint"]=point+editor->position();click(qobject_cast<QQuickItem*>(find("taskEditorTabs-segment-0")));++m_firstInspectorPhase;m_wait.restart();return {};}
        if(point+editor->position()!=m_columns.value("sourcePoint").toPointF())return fail("tab does not replace source with popup origin");
        invoke(m_window,"returnFromDetails");click(row(0));
    }
    if(m_stage==6){if(count()!=1 || selection->property("anchorId")!=m_ids[0])return fail("plain click anchor");click(row(2),Qt::ShiftModifier);}
    if(m_stage==7){if(count()!=3 || !m_window->property("batchMode").toBool() || find("taskQuickEditor")->property("editing").toBool())return fail("Shift includes visible child and closes editing");click(row(1),Qt::ControlModifier);}
    if(m_stage==8){if(count()!=2)return fail("Command/Ctrl toggles only target");click(row(2),Qt::NoModifier,Qt::RightButton);}
    if(m_stage==9){if(count()!=2 || !menu()->property("visible").toBool())return fail("right click preserves selected collection");hover(qobject_cast<QQuickItem*>(find("batchMenuComplete")));}
    if(m_stage==10){hover(qobject_cast<QQuickItem*>(find("batchMenuSummary")));}
    if(m_stage==11){if(activeFill(find("batchMenuSummary")))return fail("disabled menu header painted active");if(!capture("multi-context-light"))return fail("capture");invoke(menu(),"close");click(row(4),Qt::NoModifier,Qt::RightButton);}
    if(m_stage==12){if(count()!=1 || menu()->property("visible").toBool() || !find("taskContext-"+m_ids[4])->property("visible").toBool())return fail("outside selection right click becomes single context");invoke(find("taskContext-"+m_ids[4]),"close");list->forceActiveFocus();key(list,Qt::Key_A,Qt::ControlModifier);}
    if(m_stage==13){if(count()!=5)return fail("list select all");key(list,Qt::Key_Escape);}
    if(m_stage==14){if(m_window->property("batchMode").toBool() || count()!=0)return fail("Escape clears selection");click(row(0));}
    if(m_stage==15){auto* title=qobject_cast<QQuickItem*>(find("quickEditorTitle"));if(!title)return fail("title editor");key(title,Qt::Key_A,Qt::ControlModifier);if(title->property("selectedText")!=title->property("text") || count()!=1 || m_window->property("batchMode").toBool())return fail("text select all stays in text");invoke(find("taskQuickEditor"),"closeRequested");}
    if(m_stage==16){list->forceActiveFocus();key(list,Qt::Key_Down,Qt::ShiftModifier);}
    if(m_stage==17){if(count()!=2)return fail("keyboard range extension");if(!m_workspace.batchTasks(m_window->property("batchSelection").toList(),"complete",{}))return fail("complete selection");}
    if(m_stage==18){QMetaObject::invokeMethod(m_window,"changeView",Q_ARG(QVariant,QVariant("completed")));}
    if(m_stage==19){m_ids.clear();for(const auto& task:m_workspace.tasks()->items())m_ids.append(task.id);if(m_ids.size()!=2)return fail("completed fixture");click(row(0),Qt::NoModifier,Qt::RightButton);}
    if(m_stage==20){hover(qobject_cast<QQuickItem*>(find("taskMenuState-"+m_ids[0])));}
    if(m_stage==21){QVariant point;QMetaObject::invokeMethod(row(0),"hoverDisabledMenuPosition",Q_RETURN_ARG(QVariant,point));const auto map=point.toMap();if(map.isEmpty())return fail("disabled move entry");const QPointF at(map.value("x").toReal(),map.value("y").toReal());QMouseEvent event(QEvent::MouseMove,at,m_window->mapToGlobal(at),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(m_window,&event);}
    if(m_stage==22){QVariant result;QMetaObject::invokeMethod(row(0),"verifyMenuHighlight",Q_RETURN_ARG(QVariant,result));if(result!="PASS" || !capture("single-disabled-hover"))return fail("completed menu exclusive highlight: "+result.toString());invoke(find("taskContext-"+m_ids[0]),"close");click(row(0));}
    if(m_stage==23){click(row(1),Qt::ShiftModifier);}
    if(m_stage==24){if(count()!=2)return fail("completed Shift selection");click(row(0),Qt::NoModifier,Qt::RightButton);}
    if(m_stage==25){if(!menu()->property("visible").toBool() || !find("batchMenuReopen")->property("visible").toBool() || find("batchMenuComplete")->property("visible").toBool())return fail("completed multi context capabilities");if(!capture("completed-multi-context"))return fail("capture completed");click(qobject_cast<QQuickItem*>(find("batchMenuReopen")));}
    if(m_stage==26){auto* dialog=find("batchTaskDialog");if(!dialog->property("visible").toBool() || dialog->property("initialAction").toInt()!=4)return fail("reopen preview");click(qobject_cast<QQuickItem*>(find("applyBatchSelection")));}
    if(m_stage==27){if(m_workspace.tasks()->rowCount()!=0 || count()!=0 || find("batchTaskDialog")->property("visible").toBool())return fail("reopen atomic completion and selection cleanup");QMetaObject::invokeMethod(m_window,"changeView",Q_ARG(QVariant,QVariant("inbox")));}
    if(m_stage==28){m_ids.clear();for(const auto& task:m_workspace.tasks()->items())m_ids.append(task.id);click(row(0));}
    if(m_stage==29){click(row(3),Qt::ShiftModifier);}
    if(m_stage==30){m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_window->resize(880,580);}
    if(m_stage==31){if(count()!=4 || !capture("selection-dark-200"))return fail("selection survives density and theme");m_workspace.setSearch("no matching synthetic task");}
    if(m_stage==32){if(m_wait.elapsed()<350)return {};if(m_workspace.tasks()->rowCount()!=0 || count()!=0){if(m_wait.elapsed()<1500)return {};capture("failed-filtered-selection");return fail(QStringLiteral("selection drops filtered-out identities: rows=%1 selected=%2").arg(m_workspace.tasks()->rowCount()).arg(count()));}m_workspace.setSearch("");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_window->resize(1280,800);}
    if(m_stage==33){if(m_wait.elapsed()<350)return {};list->forceActiveFocus();key(list,Qt::Key_Escape);if(!m_workspace.createForTime(QStringLiteral("紧凑属性行验收"),m_workspace.dateOffset(0),540))return fail("timed density fixture");}
    if(m_stage==34){for(const auto& task:m_workspace.tasks()->items())if(task.title==QStringLiteral("紧凑属性行验收"))m_parent=task.id;auto* details=qobject_cast<QQuickItem*>(find("taskDetails-"+m_parent));if(!details)return fail("detail entry");m_panelFrameMs=-1;m_presentClock.start();m_awaitingPanelFrame=true;click(details);}
    if(m_stage==35){auto* editor=qobject_cast<QQuickItem*>(find("taskEditor"));auto* title=qobject_cast<QQuickItem*>(find("editTaskTitle"));auto* summary=qobject_cast<QQuickItem*>(find("editorTimeSummary"));if(!editor || !editor->isVisible() || !summary || !summary->isVisible() || title->mapToItem(editor,QPointF()).y()>64 || qAbs(title->mapToItem(editor,QPointF()).x()-qobject_cast<QQuickItem*>(find("editTaskNotes"))->mapToItem(editor,QPointF()).x())>1 || editor->height()>440){capture("failed-compact");return fail(QStringLiteral("compact header and content height: visible=%1 summary=%2 titleY=%3 height=%4 expanded=%5").arg(editor && editor->isVisible()).arg(summary && summary->isVisible()).arg(title && editor ? title->mapToItem(editor,QPointF()).y() : -1).arg(editor ? editor->height() : -1).arg(m_window->property("inspectorExpanded").toBool()));}qInfo()<<"UX37 detail first swapped frame ms="<<m_panelFrameMs<<"platform="<<QGuiApplication::platformName();if(!capture("compact-inspector"))return fail("compact capture");click(summary);}
    if(m_stage==36){auto* date=qobject_cast<QQuickItem*>(find("editStartDate"));auto* time=qobject_cast<QQuickItem*>(find("taskTimeStart"));if(!date || !time || !date->isVisible() || !time->isVisible() || qAbs(date->mapToScene(QPointF()).y()-time->mapToScene(QPointF()).y())>1 || date->mapToScene(QPointF()).x()+date->width()>time->mapToScene(QPointF()).x()+1)return fail("start date and time share a non-overlapping row");if(!capture("compact-time-expanded"))return fail("time capture");m_workspace.setPreference("compact",false);}
    if(m_stage==37){if(!capture("comfortable-inspector"))return fail("comfortable capture");m_workspace.setPreference("compact",true);m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_window->resize(880,580);}
    if(m_stage==38){auto* scroll=qobject_cast<QQuickItem*>(find("taskEditorScroll"));auto* title=qobject_cast<QQuickItem*>(find("editTaskTitle"));title->forceActiveFocus();if(!scroll || scroll->width()<200 || scroll->height()<96 || !capture("inspector-dark-200"))return fail("readable expanded inspector");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_window->resize(1280,800);}
    if(m_stage==39){auto* choice=qobject_cast<QQuickItem*>(find("taskRepeatChoice"));if(!choice || !choice->isVisible())return fail("repeat property entry");click(choice);key(choice,Qt::Key_End);key(choice,Qt::Key_Return);}
    if(m_stage==40){auto* frequency=qobject_cast<QQuickItem*>(find("taskRepeatFrequency"));if(!find("taskTimingFields")->property("repeatExpanded").toBool() || !frequency || !frequency->isVisible())return fail("custom repeat expands in the same inspector");if(!capture("custom-repeat"))return fail("repeat capture");auto* choice=qobject_cast<QQuickItem*>(find("taskRepeatChoice"));click(choice);key(choice,Qt::Key_Home);key(choice,Qt::Key_Down);key(choice,Qt::Key_Down);key(choice,Qt::Key_Return);}
    if(m_stage==41){QVariant patch;QMetaObject::invokeMethod(find("taskTimingFields"),"detailsPatch",Q_RETURN_ARG(QVariant,patch));if(patch.toMap().value("repeat").toMap().value("frequency")!="WEEKLY")return fail("repeat preset draft");invoke(m_window,"saveEditor");}
    if(m_stage==42){if(m_workspace.task(m_parent).value("repeat").toMap().value("frequency")!="WEEKLY" || !m_workspace.errorText().isEmpty())return fail("repeat preset persists with the same task");invoke(find("taskEditor"),"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_window->resize(1120,720);}
    if(m_stage==43){const auto task=m_workspace.task(m_ids[0]);if(!m_workspace.saveTask(m_ids[0],task.value("rowVersion").toLongLong(),task.value("title").toString(),QStringLiteral("准备第一版计划\n补充细节与验收安排"),"INBOX","","",false))return fail("list notes fixture");}
    if(m_stage==44){list->setProperty("currentIndex",-1);const auto parent=columns(row(0)),child=columns(row(1));if(parent.isEmpty() || child.isEmpty() || qAbs(child.value("completion").toDouble()-parent.value("completion").toDouble()-20)>1 || qAbs(child.value("title").toDouble()-parent.value("title").toDouble()-20)>1 || qAbs(parent.value("separator").toDouble()-parent.value("title").toDouble())>1 || find("taskRelation-"+m_ids[1]) || !qobject_cast<QQuickItem*>(find("taskNotes-"+m_ids[0]))->isVisible() || qobject_cast<QQuickItem*>(find("taskSubtasksLink-"+m_ids[0]))->isVisible())return fail("single hierarchy and aligned summary");if(qAbs(parent.value("completionCenterY").toReal()-parent.value("titleCenterY").toReal())>1 || qAbs(child.value("completionCenterY").toReal()-child.value("titleCenterY").toReal())>1){qInfo()<<"List title axes"<<parent<<child;return fail("browse completion centers on title line");}if(qAbs(parent.value("contentTop").toReal()-parent.value("contentBottom").toReal())>1 || parent.value("contentTop").toReal()>6)return fail("supporting text has balanced compact insets");if(!capture("list-refined-light"))return fail("list capture");m_columns=child;click(row(1));}
    if(m_stage==45){const auto positions=columns(row(1));if(!row(1)->property("expanded").toBool() || qAbs(positions.value("completion").toDouble()-m_columns.value("completion").toDouble())>1 || qAbs(positions.value("title").toDouble()-m_columns.value("title").toDouble())>1 || qAbs(positions.value("notes").toDouble()-positions.value("title").toDouble())>1 || find("saveQuickEditor")->property("visible").toBool() || !capture("list-refined-inline"))return fail("inline uses same columns without extra controls");if(qAbs(positions.value("tools").toReal()-positions.value("title").toReal())>1 || qAbs(positions.value("completionCenterY").toReal()-positions.value("titleCenterY").toReal())>1){qInfo()<<"Inline title axes"<<positions;return fail("inline tools and title line alignment");};auto* end=qobject_cast<QQuickItem*>(find("quickEditorDeadline"));if(!m_inlineDateOpened){if(end->property("iconOnly").toBool() && qAbs(end->width()-end->height())>1)return fail("empty date action is circular");click(end);m_inlineDateOpened=true;m_wait.restart();return {};}if(!end->property("pickerVisible").toBool())return fail("compact date action opens nearby picker");invoke(end,"closePicker");QMetaObject::invokeMethod(find("taskQuickEditor"),"notesEdited",Q_ARG(QString,QStringLiteral("就近编辑备注")));invoke(find("taskQuickEditor"),"closeRequested");}
    if(m_stage==46){if(find("taskQuickEditor")->property("visible").toBool() || m_workspace.task(m_ids[1]).value("notes")!=QStringLiteral("就近编辑备注") || find("taskNotes-"+m_ids[1])->property("text")!=QStringLiteral("就近编辑备注"))return fail("leave saves and updates preview");click(qobject_cast<QQuickItem*>(find("taskChildren-"+m_ids[0])));}
    if(m_stage==47){if(m_workspace.taskIndex(m_ids[1])>=0 || !qobject_cast<QQuickItem*>(find("taskSubtasksLink-"+m_ids[0]))->isVisible() || !capture("list-refined-collapsed"))return fail("collapsed hierarchy keeps child entry");click(qobject_cast<QQuickItem*>(find("taskChildren-"+m_ids[0])));}
    if(m_stage==48){if(m_workspace.taskIndex(m_ids[1])<0)return fail("children expand again");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_window->resize(880,580);}
    if(m_stage==49){const auto parent=columns(row(0)),child=columns(row(1));QVariant geometry;QMetaObject::invokeMethod(row(1),"verifyGeometry",Q_RETURN_ARG(QVariant,geometry));if(geometry!="PASS" || qAbs(child.value("title").toDouble()-parent.value("title").toDouble()-20)>1 || !capture("list-refined-dark-200"))return fail("large narrow hierarchy and content bounds");m_columns=child;click(row(1));}
    if(m_stage==50){const auto positions=columns(row(1));if(qAbs(positions.value("title").toDouble()-m_columns.value("title").toDouble())>1 || !capture("list-refined-inline-200"))return fail("large inline alignment");QMetaObject::invokeMethod(find("taskQuickEditor"),"titleEdited",Q_ARG(QString,QString()));invoke(find("taskQuickEditor"),"closeRequested");}
    if(m_stage==51){if(!find("taskQuickEditor")->property("visible").toBool() || m_workspace.errorText().isEmpty() || !capture("list-refined-invalid-title"))return fail("invalid leave retains inline draft");QMetaObject::invokeMethod(find("taskQuickEditor"),"titleEdited",Q_ARG(QString,m_workspace.task(m_ids[1]).value("title").toString()));invoke(find("taskQuickEditor"),"closeRequested");}
    if(m_stage==52){if(find("taskQuickEditor")->property("visible").toBool() || !m_workspace.errorText().isEmpty())return fail("corrected draft exits safely");m_workspace.setPreference("highContrast",true);}
    if(m_stage==53){if(!capture("list-refined-contrast-200"))return fail("contrast capture");m_workspace.setSearch(QStringLiteral("独立任务 2"));m_workspace.setTextScale(1);m_workspace.setThemeMode(1);}
    if(m_stage==54){if(m_wait.elapsed()<350)return {};if(m_workspace.tasks()->rowCount()!=1 || m_workspace.tasks()->hasHierarchy())return fail("flat search projection");const auto positions=columns(row(2));if(positions.isEmpty() || qAbs(positions.value("completion").toDouble()-6)>1 || qAbs(positions.value("title").toDouble()-46)>1 || !capture("list-refined-flat"))return fail("flat list removes unused hierarchy gutter");m_workspace.setSearch("");}
    if(m_stage==55){if(m_wait.elapsed()<350)return {};if(!m_workspace.tasks()->hasHierarchy())return fail("tree restored after search");m_workspace.setPreference("listShowSubtasks",false);}
    if(m_stage==56){if(m_workspace.taskIndex(m_ids[1])>=0 || !qobject_cast<QQuickItem*>(find("taskSubtasksLink-"+m_ids[0]))->isVisible())return fail("global hidden children retain count entry");m_workspace.setPreference("listShowSubtasks",true);}
    if(m_stage==57){if(m_workspace.taskIndex(m_ids[1])<0 || m_workspace.task(m_ids[1]).value("notes")!=QStringLiteral("就近编辑备注"))return fail("global show restores same child and data");m_window->resize(1280,800);m_workspace.setPreference("highContrast",false);m_workspace.saveCatalog({{"kind","tag"},{"title",QStringLiteral("计划与设计")}});}
    if(m_stage==58){const auto tags=m_workspace.catalogOf("tag");if(tags.isEmpty())return fail("tag fixture");m_tag=tags.last().toMap().value("id").toString();QMetaObject::invokeMethod(m_window,"openEditor",Q_ARG(QVariant,QVariant(m_ids[2])));}
    if(m_stage==59){click(qobject_cast<QQuickItem*>(find("editorTags")));}
    if(m_stage==60){auto* menu=find("editorTagMenu");auto* option=qobject_cast<QQuickItem*>(find("editorTagMenu-option-"+m_tag));auto* separator=qobject_cast<QQuickItem*>(find("editorTagMenu-separator"));auto* create=qobject_cast<QQuickItem*>(find("editorTagMenu-create"));if(!menu || !menu->property("visible").toBool() || !option || !separator || !create || option->y()>=separator->y() || separator->y()>=create->y() || !capture("tags-menu-light"))return fail("anchored tag menu order and separator");click(option);}
    }
    else if(m_stage>=61){
    if(m_stage==61){if(find("editorTagMenu")->property("visible").toBool() || !draftTags().contains(m_tag) || m_workspace.task(m_ids[2]).value("tagIds").toStringList().contains(m_tag))return fail("tag choice edits draft without full-task save");click(qobject_cast<QQuickItem*>(find("editorTags")));}
    if(m_stage==62){for(const auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent event(type,Qt::Key_Escape,Qt::NoModifier);QCoreApplication::sendEvent(m_window,&event);}}
    if(m_stage==63){if(find("editorTagMenu")->property("visible").toBool() || !find("taskEditor")->property("visible").toBool())return fail(QStringLiteral("Escape closes only tag menu: menu=%1 editor=%2").arg(find("editorTagMenu")->property("visible").toBool()).arg(find("taskEditor")->property("visible").toBool()));click(qobject_cast<QQuickItem*>(find("editorTags")));}
    if(m_stage==64){click(qobject_cast<QQuickItem*>(find("editorTagMenu-create")));}
    if(m_stage==65){if(!find("catalogCreateDialog")->property("visible").toBool())return fail("tag create menu opens existing dialog");invoke(find("catalogCreateDialog"),"close");}
    if(m_stage==66){if(!draftTags().contains(m_tag))return fail("cancel new tag preserves draft selection");click(qobject_cast<QQuickItem*>(find("editorTags")));}
    if(m_stage==67){click(qobject_cast<QQuickItem*>(find("editorTagMenu-create")));}
    if(m_stage==68){find("newCatalogName")->setProperty("text",QStringLiteral("跨项目的长标签名称用于检查菜单换行与窄窗边界"));invoke(find("catalogCreateDialog"),"create");}
    if(m_stage==69){const auto tags=m_workspace.catalogOf("tag");for(const auto& value:tags){const auto tag=value.toMap();if(tag.value("title")==QStringLiteral("跨项目的长标签名称用于检查菜单换行与窄窗边界"))m_createdTag=tag.value("id").toString();}if(m_createdTag.isEmpty() || m_createdTag==m_tag || !draftTags().contains(m_createdTag))return fail("new tag selected in original draft");invoke(m_window,"returnFromDetails");}
    if(m_stage==70){const auto selected=m_workspace.task(m_ids[2]).value("tagIds").toStringList();if(!selected.contains(m_tag) || !selected.contains(m_createdTag))return fail("leave persists both tag choices");QMetaObject::invokeMethod(m_window,"openQuickEditor",Q_ARG(QVariant,QVariant(m_ids[2])));}
    if(m_stage==71){click(qobject_cast<QQuickItem*>(find("quickTagButton")));}
    if(m_stage==72){auto* option=find("quickTagMenu-option-"+m_tag);if(!find("quickTagMenu")->property("visible").toBool() || !option || !option->property("checked").toBool())return fail("inline shares same tag selection");click(qobject_cast<QQuickItem*>(option));}
    if(m_stage==73){invoke(find("taskQuickEditor"),"closeRequested");}
    if(m_stage==74){const auto selected=m_workspace.task(m_ids[2]).value("tagIds").toStringList();if(selected.contains(m_tag) || !selected.contains(m_createdTag))return fail("inline deselection saves without dropping other tag");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_window->resize(880,580);m_window->requestActivate();QMetaObject::invokeMethod(m_window,"openEditor",Q_ARG(QVariant,QVariant(m_ids[2])));}
    if(m_stage==75){qobject_cast<QQuickItem*>(find("editorTags"))->forceActiveFocus();}
    if(m_stage==76){auto* tag=qobject_cast<QQuickItem*>(find("editorTags"));auto* scroll=qobject_cast<QQuickItem*>(find("taskEditorScroll"));const auto y=tag->mapToItem(scroll,QPointF()).y();if(y<0 || y+tag->height()>scroll->height()){if(m_wait.elapsed()<1500)return {};return fail("keyboard reveals tag source before opening menu");}click(tag);}
    if(m_stage==77){auto* option=qobject_cast<QQuickItem*>(find("editorTagMenu-option-"+m_createdTag));const auto pos=option->mapToScene(QPointF());if(!find("editorTagMenu")->property("visible").toBool() || !option->property("checked").toBool() || pos.x()<0 || pos.x()+option->width()>m_window->width() || pos.y()<0 || pos.y()+option->height()>m_window->height() || !capture("tags-menu-dark-200"))return fail("large narrow tag menu bounds");invoke(find("editorTagMenu"),"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_window->resize(1280,800);invoke(m_window,"returnFromDetails");}
    if(m_stage==78){QMetaObject::invokeMethod(m_window,"openQuickEditor",Q_ARG(QVariant,QVariant(m_ids[2])));}
    if(m_stage==79){m_columns["important"]=m_workspace.task(m_ids[2]).value("important");click(qobject_cast<QQuickItem*>(find("quickImportant")));}
    if(m_stage==80){if(find("taskDetailsFields")->property("importantValue").toBool()==m_columns.value("important").toBool() || m_workspace.task(m_ids[2]).value("important")!=m_columns.value("important"))return fail("inline flag edits draft only");windowKey(Qt::Key_F,Qt::ControlModifier|Qt::ShiftModifier);}
    if(m_stage==81){if(find("taskDetailsFields")->property("importantValue")!=m_columns.value("important"))return fail("flag shortcut toggles same draft");invoke(find("taskQuickEditor"),"closeRequested");}
    if(m_stage==82){QMetaObject::invokeMethod(selection,"single",Q_ARG(QVariant,QVariant(m_ids[2])));QMetaObject::invokeMethod(selection,"toggle",Q_ARG(QVariant,QVariant(m_ids[3])));m_window->setProperty("batchMode",true);list->forceActiveFocus();m_window->requestActivate();}
    if(m_stage==83){windowKey(Qt::Key_F,Qt::ControlModifier|Qt::ShiftModifier);}
    if(m_stage==84){if(!m_workspace.task(m_ids[2]).value("important").toBool() || !m_workspace.task(m_ids[3]).value("important").toBool() || m_workspace.task(m_ids[4]).value("important").toBool() || count()!=2)return fail("batch flag affects only selection");windowKey(Qt::Key_F,Qt::ControlModifier|Qt::ShiftModifier);}
    if(m_stage==85){if(m_workspace.task(m_ids[2]).value("important").toBool() || m_workspace.task(m_ids[3]).value("important").toBool() || !m_workspace.errorText().isEmpty())return fail("own flag updates rebase selected snapshots");auto* title=find("newTaskTitle");if(!title)title=find("taskTitleInput");if(title){title->setProperty("text","abc");title->setProperty("cursorPosition",0);qobject_cast<QQuickItem*>(title)->forceActiveFocus();}m_columns["titleObject"]=title?title->objectName():QString();windowKey(Qt::Key_Delete);}
    if(m_stage==86){if(find("batchTaskDialog")->property("visible").toBool())return fail("Delete in text does not delete tasks");const auto title=m_columns.value("titleObject").toString();if(title.isEmpty() || find(title)->property("text")!="bc")return fail("Delete still edits the active text input");find(title)->setProperty("text","");list->forceActiveFocus();windowKey(Qt::Key_Delete);}
    if(m_stage==87){auto* dialog=find("batchTaskDialog");if(!dialog->property("visible").toBool() || !dialog->property("deleting").toBool() || dialog->property("affectedCount").toInt()!=2)return fail("Delete opens accurate batch confirmation");windowKey(Qt::Key_Escape);}
    if(m_stage==88){if(find("batchTaskDialog")->property("visible").toBool() || count()!=2)return fail("cancel deletion preserves selection");click(row(2),Qt::NoModifier,Qt::RightButton);}
    if(m_stage==89){if(!menu()->property("visible").toBool())return fail("batch context deletion fixture");click(qobject_cast<QQuickItem*>(find("batchMenuDelete")));}
    if(m_stage==90){if(!find("batchTaskDialog")->property("deleting").toBool() || !capture("batch-delete-confirm"))return fail("context menu shares deletion confirmation");click(qobject_cast<QQuickItem*>(find("applyBatchSelection")));}
    if(m_stage==91){if(m_workspace.taskIndex(m_ids[2])>=0 || m_workspace.taskIndex(m_ids[3])>=0 || m_workspace.taskIndex(m_ids[4])<0 || !m_workspace.canUndo() || count()!=0)return fail("batch soft deletion preserves unrelated task");click(qobject_cast<QQuickItem*>(find("undoTaskDeletion")));}
    if(m_stage==92){if(m_workspace.taskIndex(m_ids[2])<0 || m_workspace.taskIndex(m_ids[3])<0 || m_workspace.canUndo() || !m_workspace.errorText().isEmpty())return fail("batch deletion undo restores exact tasks");QMetaObject::invokeMethod(m_window,"openEditor",Q_ARG(QVariant,QVariant(m_ids[0])));}
    if(m_stage==93){auto* tabs=qobject_cast<QQuickItem*>(find("taskEditorTabs"));auto* editor=qobject_cast<QQuickItem*>(find("taskEditor"));if(!tabs || !tabs->isVisible() || tabs->width()>editor->width()-24 || tabs->height()<32 || !capture("inspector-tabs-light"))return fail("compact inspector content tabs");click(qobject_cast<QQuickItem*>(find("taskEditorTabs-segment-1")));}
    if(m_stage==94){if(m_window->property("editorTab").toInt()!=1 || !qobject_cast<QQuickItem*>(find("subtaskList"))->isVisible() || !capture("inspector-tabs-subtasks"))return fail("tab pointer selects children");auto* tab=qobject_cast<QQuickItem*>(find("taskEditorTabs-segment-1"));tab->forceActiveFocus();key(tab,Qt::Key_Left);}
    if(m_stage==95){if(m_window->property("editorTab").toInt()!=0 || !qobject_cast<QQuickItem*>(find("taskEditorTabs-segment-0"))->hasActiveFocus())return fail("tab arrow keeps keyboard focus");find("editTaskNotes")->setProperty("text",QStringLiteral("页签切换保存同一草稿"));auto* tab=qobject_cast<QQuickItem*>(find("taskEditorTabs-segment-0"));tab->forceActiveFocus();key(tab,Qt::Key_End);}
    if(m_stage==96){if(m_window->property("editorTab").toInt()!=1 || m_workspace.task(m_ids[0]).value("notes")!=QStringLiteral("页签切换保存同一草稿"))return fail("tab transition retains atomic save gate");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_window->resize(880,580);}
    if(m_stage==97){auto* tabs=qobject_cast<QQuickItem*>(find("taskEditorTabs"));auto* editor=qobject_cast<QQuickItem*>(find("taskEditor"));if(!tabs->isVisible() || tabs->width()>editor->width()-24 || tabs->mapToItem(editor,QPointF()).y()+tabs->height()>editor->height() || !capture("inspector-tabs-subtasks-dark-200"))return fail("large content tabs bounded");auto* tab=qobject_cast<QQuickItem*>(find("taskEditorTabs-segment-1"));tab->forceActiveFocus();key(tab,Qt::Key_Home);}
    if(m_stage==98){if(m_window->property("editorTab").toInt()!=0 || !capture("inspector-tabs-dark-200"))return fail("tab Home selects details");find("editTaskTitle")->setProperty("text",QString());auto* tab=qobject_cast<QQuickItem*>(find("taskEditorTabs-segment-0"));tab->forceActiveFocus();key(tab,Qt::Key_End);}
    if(m_stage==101){if(find("taskEditor")->property("visible").toBool())return fail("content tabs keep close semantics");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_window->resize(1280,800);}
    if(m_stage==99){if(m_window->property("editorTab").toInt()!=0 || !find("taskEditorTabs-segment-0")->property("checked").toBool() || find("taskEditorTabs-segment-1")->property("checked").toBool() || m_workspace.errorText().isEmpty() || !find("editTaskTitle")->property("text").toString().isEmpty())return fail("failed save keeps original tab and invalid draft");find("editTaskTitle")->setProperty("text",m_workspace.task(m_ids[0]).value("title"));auto* tab=qobject_cast<QQuickItem*>(find("taskEditorTabs-segment-0"));tab->forceActiveFocus();key(tab,Qt::Key_End);}
    if(m_stage==100){if(m_window->property("editorTab").toInt()!=1 || !m_workspace.errorText().isEmpty())return fail("repaired draft switches tab after saving");invoke(m_window,"returnFromDetails");}
    if(m_stage==102){QMetaObject::invokeMethod(m_window,"openEditor",Q_ARG(QVariant,QVariant(m_ids[0])));}
    if(m_stage==103){auto* editor=qobject_cast<QQuickItem*>(find("taskEditor"));auto* grip=qobject_cast<QQuickItem*>(find("taskEditorDragArea"));m_columns["panel"]=editor->position();m_columns["version"]=m_workspace.task(m_ids[0]).value("rowVersion");const auto at=grip->mapToScene(QPointF(grip->width()/2,grip->height()/2));pointer(QEvent::MouseButtonPress,at);pointer(QEvent::MouseMove,at+QPointF(70,50));pointer(QEvent::MouseButtonRelease,at+QPointF(70,50));}
    if(m_stage==104){auto* editor=qobject_cast<QQuickItem*>(find("taskEditor"));if(editor->position()!=m_columns.value("panel").toPointF()+QPointF(70,50) || !find("taskEditorSurface")->property("arrowEdge").toString().isEmpty() || m_workspace.task(m_ids[0]).value("rowVersion")!=m_columns.value("version") || !capture("inspector-detached-drag"))return fail("grip moves panel, detaches arrow and writes no task");m_columns["panel"]=editor->position();auto* grip=qobject_cast<QQuickItem*>(find("taskEditorDragArea"));const auto at=grip->mapToScene(QPointF(grip->width()/2,grip->height()/2));pointer(QEvent::MouseButtonPress,at);pointer(QEvent::MouseMove,at+QPointF(-40,-25));windowKey(Qt::Key_Escape);pointer(QEvent::MouseButtonRelease,at+QPointF(-40,-25));}
    if(m_stage==105){auto* editor=qobject_cast<QQuickItem*>(find("taskEditor"));if(!editor->isVisible() || editor->position()!=m_columns.value("panel").toPointF())return fail("Escape cancels only active drag");auto* grip=qobject_cast<QQuickItem*>(find("taskEditorDragHandle"));grip->forceActiveFocus();key(grip,Qt::Key_Right,Qt::ShiftModifier);}
    if(m_stage==106){auto* editor=qobject_cast<QQuickItem*>(find("taskEditor"));if(editor->x()!=m_columns.value("panel").toPointF().x()+32)return fail("keyboard move alternative");m_columns["panel"]=editor->position();click(qobject_cast<QQuickItem*>(find("taskEditorTabs-segment-1")));}
    if(m_stage==107){auto* editor=qobject_cast<QQuickItem*>(find("taskEditor"));if(editor->position()!=m_columns.value("panel").toPointF() || m_window->property("editorTab").toInt()!=1)return fail("manual position preserved through tabs");m_window->resize(1040,640);}
    if(m_stage==108){auto* editor=qobject_cast<QQuickItem*>(find("taskEditor"));if(editor->x()<m_window->property("sidebarWidth").toReal()+12 || editor->x()+editor->width()>m_window->width()-12 || editor->y()<12 || editor->y()+editor->height()>m_window->height()-12)return fail("manual panel clamps after resize");invoke(m_window,"returnFromDetails");m_window->resize(1280,800);QMetaObject::invokeMethod(m_window,"clearTaskSelection");const auto selected=m_workspace.tasks()->selectionEntries(0,m_workspace.tasks()->rowCount()-1);if(!m_workspace.batchTasks(selected,"delete",{}))return fail("trash fixture");}
    if(m_stage==109){QMetaObject::invokeMethod(m_window,"changeView",Q_ARG(QVariant,QVariant("trash")));}
    if(m_stage==110){m_ids.clear();for(const auto& task:m_workspace.tasks()->items())m_ids.append(task.id);if(m_ids.size()<4)return fail("deleted fixture size");click(row(0));}
    if(m_stage==111){if(count()!=1 || m_window->property("editingActive").toBool())return fail("trash plain click selects without editing");click(row(2),Qt::ShiftModifier);}
    if(m_stage==112){if(count()!=3 || !m_window->property("batchMode").toBool())return fail("trash Shift range");click(row(1),Qt::ControlModifier);}
    if(m_stage==113){if(count()!=2)return fail("trash Ctrl toggle");click(row(2),Qt::NoModifier,Qt::RightButton);}
    if(m_stage==114){if(count()!=2 || !find("trashTaskContext")->property("visible").toBool() || menu()->property("visible").toBool() || !find("trashMenuSummary")->property("visible").toBool() || !capture("trash-multi-context"))return fail("trash right click collection context only");click(qobject_cast<QQuickItem*>(find("trashMenuRestore")));}
    if(m_stage==115){if(!find("batchTaskDialog")->property("visible").toBool() || !find("batchTaskDialog")->property("restoring").toBool() || find("batchTaskDialog")->property("affectedCount").toInt()<2 || !capture("trash-restore-preview"))return fail("restore affected preview");invoke(find("batchTaskDialog"),"close");}
    if(m_stage==116){if(count()!=2 || m_workspace.tasks()->rowCount()!=m_ids.size())return fail("cancel restore preserves selection and data");click(row(3),Qt::NoModifier,Qt::RightButton);}
    if(m_stage==117){if(count()!=1 || !find("trashTaskContext")->property("visible").toBool() || find("trashMenuSummary")->property("visible").toBool() || !capture("trash-single-context"))return fail("trash outside selection uses single restore menu");invoke(find("trashTaskContext"),"close");list->forceActiveFocus();key(list,Qt::Key_A,Qt::ControlModifier);}
    if(m_stage==118){if(count()!=m_workspace.tasks()->rowCount())return fail("trash select all");windowKey(Qt::Key_Delete);}
    if(m_stage==119){if(find("batchTaskDialog")->property("visible").toBool() || m_workspace.tasks()->rowCount()!=m_ids.size())return fail("trash Delete cannot repeat soft delete");key(list,Qt::Key_F10,Qt::ShiftModifier);}
    if(m_stage==120){if(!find("trashTaskContext")->property("visible").toBool() || count()!=m_ids.size())return fail("trash keyboard collection menu");click(qobject_cast<QQuickItem*>(find("trashMenuRestore")));}
    if(m_stage==121){click(qobject_cast<QQuickItem*>(find("applyBatchSelection")));}
    if(m_stage==122){if(m_workspace.tasks()->rowCount()!=0 || count()!=0 || find("batchTaskDialog")->property("visible").toBool() || !m_workspace.errorText().isEmpty())return fail("restore collection commits then clears selected identities");QMetaObject::invokeMethod(m_window,"changeView",Q_ARG(QVariant,QVariant("inbox")));}
    if(m_stage==123){if(m_workspace.tasks()->rowCount()!=m_ids.size())return fail("restored identities return to list");return "DONE";}
    }
    qInfo()<<"UX37 scenario"<<m_stage<<"complete";++m_stage;m_wait.restart();return {};
}
}

#include "AppRuntime.h"
#include "SoakRunner.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QMessageBox>
#include <QQmlError>
#include <QQmlIncubator>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <QDir>
#include <QStandardPaths>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QSet>
#include <QPointer>
#include <QIcon>

namespace daymark {

bool AppRuntime::start(bool smokeTest, const QString& dataDirectory, const QString& screenshotPath,int soakSeconds,const QString& soakReport)
{
    QString directory=dataDirectory;
    if(smokeTest) {
        m_temporaryData=std::make_unique<QTemporaryDir>();
        if(!m_temporaryData->isValid())return false;
        directory=m_temporaryData->path();
    } else if(directory.isEmpty()) directory=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if(directory.isEmpty())return false;
    const auto outcome=m_instance.acquire(directory);
    if(outcome==SingleInstance::Outcome::ActivatedExisting) {m_startupExitCode=0;return false;}
    if(outcome==SingleInstance::Outcome::Failed) {
        if(!smokeTest)QMessageBox::critical(nullptr,QObject::tr("Daymark 无法启动"),QObject::tr("无法锁定数据目录或激活现有实例。请检查目录权限，并确认已有Daymark能响应。未打开数据库。"));
        return false;
    }
    QObject::connect(&m_engine, &QQmlEngine::warnings, &m_engine,
                     [this](const QList<QQmlError>&) { m_qmlWarnings = true; });
    m_startup=std::make_unique<StartupController>(smokeTest?QDir(directory).filePath(QStringLiteral("startup-test.ini")):QString());
    m_engine.setInitialProperties({{QStringLiteral("startupController"),QVariant::fromValue(m_startup.get())},{QStringLiteral("widgetController"), QVariant::fromValue(&m_widget)},
                                   {QStringLiteral("taskWorkspace"),QVariant::fromValue(&m_workspace)}});
    m_engine.loadFromModule(QStringLiteral("Daymark"), QStringLiteral("Main"));
    if (m_engine.rootObjects().isEmpty()) {
        if (!smokeTest) {
            QMessageBox::critical(nullptr, QObject::tr("Daymark 启动失败"),
                                  QObject::tr("界面资源无法加载。请检查 Qt 运行库是否完整，并重新构建程序。"));
        }
        return false;
    }

    auto* widgetWindow = m_engine.rootObjects().first()->findChild<QQuickWindow*>(QStringLiteral("widgetWindow"));
    m_widget.configureSettings(QDir(directory).filePath(QStringLiteral("ui.ini")));
    if (!m_widget.attachWindow(widgetWindow) || !m_widget.setVisible(m_widget.initialVisible())) {
        if (!smokeTest)
            QMessageBox::critical(nullptr, QObject::tr("Daymark 组件启动失败"), m_widget.errorText());
        return false;
    }

    auto* mainWindow=qobject_cast<QQuickWindow*>(m_engine.rootObjects().first());
    if(!smokeTest)m_mainState=std::make_unique<WindowStateStore>(mainWindow,QDir(directory).filePath(QStringLiteral("ui.ini")));
    if(m_mainState)QObject::connect(m_mainState.get(),&WindowStateStore::saveFailed,&m_workspace,&TaskWorkspace::settingsFailed);
    auto* desktopCalendar=mainWindow->findChild<QWindow*>(QStringLiteral("desktopCalendarWindow"));
    m_calendarState=std::make_unique<WindowStateStore>(desktopCalendar,QDir(directory).filePath(QStringLiteral("ui.ini")),nullptr,QStringLiteral("desktopCalendar"),QSize(420,480));
    QObject::connect(m_calendarState.get(),&WindowStateStore::saveFailed,&m_workspace,&TaskWorkspace::settingsFailed);
    if(!smokeTest)m_tray=std::make_unique<DesktopTray>(m_widget,mainWindow);
    QObject::connect(&m_workspace,&TaskWorkspace::reminderReady,&m_engine,[this](const QString& title,const QString& message){
        if(m_tray)m_tray->notify(title,message);
    });
    QObject::connect(qApp,&QCoreApplication::aboutToQuit,&m_engine,[this]{m_widget.saveSettings();});
    QObject::connect(&m_instance,&SingleInstance::activationRequested,&m_engine,[mainWindow]{
        if(mainWindow) {mainWindow->showNormal();mainWindow->requestActivate();}
    });
    m_workspace.start(QDir(directory).filePath(QStringLiteral("daymark.sqlite")));
    m_systemEvents=std::make_unique<SystemEventFilter>([this]{m_workspace.refreshClock();m_widget.normalizeGeometry();if(m_mainState)m_mainState->normalize();});
    QObject::connect(qGuiApp,&QGuiApplication::applicationStateChanged,&m_workspace,[this](Qt::ApplicationState state){if(state==Qt::ApplicationActive)m_systemEvents->requestRefresh();});

    if (smokeTest) {
        auto* probe=new QTimer(&m_engine);
        auto smokePhase=std::make_shared<int>(0);
        probe->setInterval(100);
        QObject::connect(probe,&QTimer::timeout,&m_engine,[this,probe,mainWindow,screenshotPath,smokePhase,soakSeconds,soakReport,phase=0,taskId=QString(),secondId=QString(),eventId=QString(),linkedTaskId=QString()]() mutable {
            *smokePhase=phase;
            if(m_workspace.busy())return;
            auto fail=[probe](const char* stage){qCritical()<<"Basic UI smoke failed at"<<stage;probe->stop();QCoreApplication::exit(2);};
            if(!m_workspace.ready()) {fail("database initialization");return;}
            auto* input=mainWindow->findChild<QObject*>(QStringLiteral("newTaskTitle"));
            auto* editTitle=mainWindow->findChild<QObject*>(QStringLiteral("editTaskTitle"));
            auto* startDate=mainWindow->findChild<QObject*>(QStringLiteral("editStartDate"));
            auto* editor=mainWindow->findChild<QObject*>(QStringLiteral("taskEditor"));
            if(!input || !editTitle || !startDate || !editor){fail("UI objects");return;}
            if(phase==0) {
                const auto icon=QGuiApplication::windowIcon();
                if(icon.isNull() || icon.pixmap(16,16).isNull() || icon.pixmap(256,256).isNull()) {fail("application icon resources");return;}
                // Start-Process -WindowStyle Hidden suppresses the first Win32 show.
                // Native focus/layout checks require an exposed rendering surface.
                if(QGuiApplication::platformName()!=QStringLiteral("offscreen")) {
                    mainWindow->hide();mainWindow->show();mainWindow->requestActivate();
                }
                if(!screenshotPath.isEmpty())mainWindow->resize(880,580);
                m_workspace.setView(QStringLiteral("today"));
                input->setProperty("text",QStringLiteral("准备今日工作"));
                if(!QMetaObject::invokeMethod(input,"accepted")){fail("quick entry");return;}
            } else if(phase==1) {
                if(m_workspace.tasks()->rowCount()!=1 || m_workspace.todayTasks()->rowCount()!=1 || !input->property("text").toString().isEmpty()) {fail("create and widget sync");return;}
                taskId=m_workspace.tasks()->data(m_workspace.tasks()->index(0),TaskListModel::Id).toString();
                if(!QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(taskId)))) {fail("open editor");return;}
                editTitle->setProperty("text",QStringLiteral("梳理今天的重点事项"));
                startDate->setProperty("text",QStringLiteral("2026-02-30"));
                if(!QMetaObject::invokeMethod(mainWindow,"saveEditor")){fail("save request");return;}
            } else if(phase==2) {
                if(m_workspace.errorText().isEmpty() || !editor->property("visible").toBool()
                    || editTitle->property("text").toString()!=QStringLiteral("梳理今天的重点事项")
                    || m_workspace.task(taskId).value(QStringLiteral("title")).toString()!=QStringLiteral("准备今日工作")) {fail("invalid date preserves draft");return;}
                if(!screenshotPath.isEmpty()) {
                    if(!mainWindow->grabWindow().save(screenshotPath+QStringLiteral(".editor.png"))) {fail("editor screenshot");return;}
                    mainWindow->resize(1120,720);
                }
                startDate->setProperty("text",QStringLiteral(""));
                if(!QMetaObject::invokeMethod(mainWindow,"saveEditor")){fail("retry save");return;}
            } else if(phase==3) {
                if(editor->property("visible").toBool() || m_workspace.task(taskId).value(QStringLiteral("title")).toString()!=QStringLiteral("梳理今天的重点事项")) {fail("edit commit");return;}
                m_workspace.changeState(taskId,m_workspace.task(taskId).value(QStringLiteral("rowVersion")).toLongLong(),QStringLiteral("complete"));
            } else if(phase==4) {
                if(m_workspace.todayTasks()->rowCount()!=0){fail("complete sync");return;}
                m_workspace.setView(QStringLiteral("logbook"));
                if(m_workspace.tasks()->rowCount()!=1){fail("logbook");return;}
                m_workspace.changeState(taskId,m_workspace.task(taskId).value(QStringLiteral("rowVersion")).toLongLong(),QStringLiteral("reopen"));
            } else if(phase==5) {
                m_workspace.setView(QStringLiteral("today"));
                if(m_workspace.todayTasks()->rowCount()!=1){fail("reopen");return;}
                m_workspace.changeState(taskId,m_workspace.task(taskId).value(QStringLiteral("rowVersion")).toLongLong(),QStringLiteral("delete"));
            } else if(phase==6) {
                if(m_workspace.todayTasks()->rowCount()!=0 || !m_workspace.canUndo()){fail("soft delete");return;}
                m_workspace.undoDelete();
            } else if(phase==7) {
                if(m_workspace.todayTasks()->rowCount()!=1 || m_workspace.canUndo()){fail("undo");return;}
                if(!QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(taskId)))) {fail("reopen editor");return;}
                editTitle->setProperty("text",QStringLiteral("未保存草稿"));
                mainWindow->close();
            } else if(phase==8) {
                auto* guard=mainWindow->findChild<QObject*>(QStringLiteral("unsavedChangesDialog"));
                auto* cancel=mainWindow->findChild<QObject*>(QStringLiteral("cancelLeave"));
                if(!guard || !guard->property("visible").toBool() || !mainWindow->isVisible() || !cancel) {fail("unsaved close protection");return;}
                if(!QMetaObject::invokeMethod(cancel,"clicked")) {fail("cancel leaving");return;}
            } else if(phase==9) {
                auto* guard=mainWindow->findChild<QObject*>(QStringLiteral("unsavedChangesDialog"));
                auto* back=mainWindow->findChild<QObject*>(QStringLiteral("returnFromEditor"));
                if(guard->property("visible").toBool() || !editor->property("visible").toBool() || !back) {fail("cancel preserves editor");return;}
                if(!QMetaObject::invokeMethod(back,"clicked")) {fail("return with draft");return;}
            } else if(phase==10) {
                auto* guard=mainWindow->findChild<QObject*>(QStringLiteral("unsavedChangesDialog"));
                auto* discard=mainWindow->findChild<QObject*>(QStringLiteral("discardDraft"));
                if(!guard->property("visible").toBool() || !discard) {fail("unsaved return protection");return;}
                if(!QMetaObject::invokeMethod(discard,"clicked")) {fail("discard draft");return;}
            } else if(phase==11) {
                if(editor->property("visible").toBool() || m_workspace.task(taskId).value(QStringLiteral("title")).toString()!=QStringLiteral("梳理今天的重点事项")) {fail("discard does not mutate database");return;}
            } else if(phase>=12 && phase<=24) {
                if(phase==12) {
                    const auto task=m_workspace.task(taskId);
                    m_workspace.saveTask(taskId,task.value(QStringLiteral("rowVersion")).toLongLong(),task.value(QStringLiteral("title")).toString(),QString(),QStringLiteral("ACTIVE"),m_workspace.dateOffset(0),m_workspace.dateOffset(2),false);
                }
                const QStringList pages={QStringLiteral("month"),QStringLiteral("week"),QStringLiteral("details"),QStringLiteral("organization"),QStringLiteral("recurrence"),QStringLiteral("settings"),QStringLiteral("data"),QStringLiteral("date"),QStringLiteral("today"),QStringLiteral("editor"),QStringLiteral("today"),QStringLiteral("editor"),QStringLiteral("today")};
                if(phase>12 && !screenshotPath.isEmpty()) {
                    const auto name=pages[phase-13]+QStringLiteral("-%1.png").arg(phase-13);
                    if(!mainWindow->grabWindow().save(screenshotPath+QLatin1Char('.')+name)){fail("feature screenshot");return;}
                }
                if(phase==20)m_workspace.setThemeMode(2);
                if(phase==22)m_workspace.setTextScale(2.0);
                if(phase==24){m_workspace.setTextScale(1.0);m_workspace.setThemeMode(1);}
                if(!QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(pages[phase-12])),Q_ARG(QVariant,QVariant(taskId)))){fail("feature pages");return;}
            } else if(phase>=132) {
                auto* extra=mainWindow->findChild<QObject*>(QStringLiteral("taskDetailsFields"));
                auto* newStep=mainWindow->findChild<QQuickItem*>(QStringLiteral("editorNewChecklistItem"));
                auto* reminder=mainWindow->findChild<QObject*>(QStringLiteral("editorReminder"));
                auto* save=mainWindow->findChild<QObject*>(QStringLiteral("saveTaskEditor"));
                auto* scroll=mainWindow->findChild<QQuickItem*>(QStringLiteral("taskEditorScroll"));
                auto* notes=mainWindow->findChild<QObject*>(QStringLiteral("editTaskNotes"));
                if(!extra || !newStep || !reminder || !save || !scroll || !notes){fail("unified editor objects");return;}
                auto capture=[&](const QString& suffix){return screenshotPath.isEmpty() || mainWindow->grabWindow().save(screenshotPath+QLatin1Char('.')+suffix+QStringLiteral(".png"));};
                auto open=[&]{return QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));};
                auto click=[&](const char* name){auto* button=mainWindow->findChild<QObject*>(QString::fromLatin1(name));return button && QMetaObject::invokeMethod(button,"clicked");};
                auto navigate=[&]{QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("anytime"))));};
                auto* flick=static_cast<QObject*>(scroll);
                const auto rowName=QStringLiteral("taskRow-")+secondId;
                std::function<QQuickItem*(QQuickItem*)> findRow=[&](QQuickItem* item)->QQuickItem*{if(!item)return nullptr;if(item->objectName()==rowName)return item;for(auto* child:item->childItems())if(auto* found=findRow(child))return found;return nullptr;};
                if(phase==132){m_workspace.setView(QStringLiteral("inbox"));m_workspace.createTask(QStringLiteral("统一编辑原始"));}
                else if(phase==133){
                    secondId=m_workspace.tasks()->data(m_workspace.tasks()->index(0),TaskListModel::Id).toString();
                    if(secondId.isEmpty() || !open()){fail("unified editor open");return;}
                    editTitle->setProperty("text",QStringLiteral("统一编辑完成"));newStep->setProperty("text",QStringLiteral("步骤 A"));QMetaObject::invokeMethod(newStep,"textEdited");
                    reminder->setProperty("text",QStringLiteral("invalid"));QMetaObject::invokeMethod(save,"clicked");
                } else if(phase==134){
                    const auto task=m_workspace.task(secondId);
                    if(!editor->property("visible").toBool() || m_workspace.errorText().isEmpty() || task.value(QStringLiteral("title")).toString()!=QStringLiteral("统一编辑原始") || !task.value(QStringLiteral("checklist")).toList().isEmpty() || editTitle->property("text").toString()!=QStringLiteral("统一编辑完成")){fail("unified failed save preserves all drafts");return;}
                    if(!capture(QStringLiteral("unified-error"))){fail("fixed editor error");return;}
                    reminder->setProperty("text",QString());QMetaObject::invokeMethod(save,"clicked");
                } else if(phase==135){
                    const auto task=m_workspace.task(secondId);
                    if(editor->property("visible").toBool() || task.value(QStringLiteral("title")).toString()!=QStringLiteral("统一编辑完成") || task.value(QStringLiteral("checklist")).toList().size()!=1 || task.value(QStringLiteral("rowVersion")).toLongLong()!=2){fail("unified atomic UI save");return;}
                    open();newStep->setProperty("text",QStringLiteral("取消后的草稿"));QMetaObject::invokeMethod(newStep,"textEdited");navigate();
                } else if(phase==136){if(!click("cancelLeave")){fail("unified cancel navigation");return;}}
                else if(phase==137){if(!editor->property("visible").toBool() || newStep->property("text").toString()!=QStringLiteral("取消后的草稿")){fail("unified checklist draft retained");return;}navigate();}
                else if(phase==138){if(!click("discardDraft")){fail("unified discard navigation");return;}}
                else if(phase==139){
                    if(editor->property("visible").toBool() || m_workspace.task(secondId).value(QStringLiteral("checklist")).toList().size()!=1){fail("unified discard zero mutation");return;}
                    m_workspace.setView(QStringLiteral("inbox"));open();notes->setProperty("text",QStringLiteral("长备注验证，单一滚动轴。\n").repeated(24));QMetaObject::invokeMethod(newStep,"forceActiveFocus");m_workspace.refreshClock();
                } else if(phase>=140 && phase<=143){
                    // Allow one rendered layout and the focus-follow timer to settle
                    // before inspecting text-scale/viewport-dependent geometry.
                    static int renderedPhase=-1;
                    static int settledFrames=0;
                    mainWindow->grabWindow();
                    if(renderedPhase!=phase){renderedPhase=phase;settledFrames=0;return;}
                    if(++settledFrames<3)return;
                    // The offscreen plugin has no active window/focus item. Exercise the
                    // scroll target directly there; Windows verifies real QQuick focus.
                    if(QGuiApplication::platformName()==QStringLiteral("offscreen"))
                        QMetaObject::invokeMethod(mainWindow,"ensureEditorFocus",Q_ARG(QVariant,QVariant::fromValue(newStep)));
                    int scrollingBodies=0;
                    std::function<void(QQuickItem*)> countBodies=[&](QQuickItem* item){if(!item || !item->isVisible())return;if(item->inherits("QQuickFlickable"))++scrollingBodies;for(auto* child:item->childItems())countBodies(child);};
                    countBodies(qobject_cast<QQuickItem*>(editor));
                    const auto fieldTop=newStep->mapToScene(QPointF(0,0)).y();const auto viewportTop=scroll->mapToScene(QPointF(0,0)).y();
                    if(scrollingBodies!=1 || !flick || flick->property("contentHeight").toDouble()<=flick->property("height").toDouble() || fieldTop<viewportTop-1 || fieldTop+newStep->height()>viewportTop+scroll->height()+1){qCritical()<<"phase"<<phase<<"bodies"<<scrollingBodies<<"top"<<fieldTop<<viewportTop<<"scroll"<<flick->property("contentY")<<flick->property("contentHeight")<<flick->property("height")<<"focus"<<mainWindow->activeFocusItem()<<newStep->hasActiveFocus();fail("single editor scroll and visible keyboard focus");return;}
                    if(phase==140){if(!capture(QStringLiteral("unified-fields"))){fail("unified fields capture");return;}mainWindow->resize(880,580);}
                    else if(phase==141){if(!capture(QStringLiteral("unified-page"))){fail("unified page capture");return;}m_workspace.setTextScale(2);}
                    else if(phase==142){if(!capture(QStringLiteral("unified-page-200"))){fail("large unified page capture");return;}m_workspace.setTextScale(1);mainWindow->resize(1440,900);}
                    else {if(!capture(QStringLiteral("unified-wide"))){fail("wide unified capture");return;}mainWindow->setProperty("draftDirty",false);QMetaObject::invokeMethod(editor,"close");mainWindow->resize(1120,720);auto* list=mainWindow->findChild<QObject*>(QStringLiteral("taskList"));if(list){QMetaObject::invokeMethod(list,"positionViewAtBeginning");QMetaObject::invokeMethod(list,"forceLayout");}}
                } else if(phase==144){auto* row=findRow(mainWindow->contentItem());if(!row || !QMetaObject::invokeMethod(row,"openVerificationMenu")){fail("task row menu open");return;}}
                else if(phase==145){
                    auto* row=findRow(mainWindow->contentItem());QVariant result;
                    if(!row || !QMetaObject::invokeMethod(row,"verifyMenuSpacing",Q_RETURN_ARG(QVariant,result)) || result.toString()!=QStringLiteral("PASS")){qCritical()<<result;fail("hidden task menu items leave no gaps");return;}
                    if(!screenshotPath.isEmpty())for(auto* window:QGuiApplication::allWindows())if(auto* popup=qobject_cast<QQuickWindow*>(window);popup && popup!=mainWindow && popup->isVisible() && popup->objectName()!=QStringLiteral("widgetWindow"))if(!popup->grabWindow().save(screenshotPath+QStringLiteral(".task-menu.png"))){fail("task menu capture");return;}
                    QMetaObject::invokeMethod(row,"closeVerificationMenu");
                } else if(phase==146)m_workspace.setView(QStringLiteral("today"));
                else if(phase==147) {
                    m_workspace.setView(QStringLiteral("inbox"));open();notes->setProperty("text",QStringLiteral("短备注"));
                    QMetaObject::invokeMethod(editTitle,"forceActiveFocus");
                    QMetaObject::invokeMethod(mainWindow,"ensureEditorFocus",Q_ARG(QVariant,QVariant::fromValue(editTitle)));
                } else if(phase==148 || phase==149) {
                    mainWindow->grabWindow();
                    auto* target=phase==148 ? qobject_cast<QQuickItem*>(notes) : mainWindow->findChild<QQuickItem*>(QStringLiteral("editorPlanning"));
                    if(!target){fail("wheel target");return;}
                    if(phase==148) {
                        static bool noteFocusPrepared=false;static int noteFocusFrames=0;
                        if(!noteFocusPrepared) {
                            QMetaObject::invokeMethod(target,"forceActiveFocus");
                            QMetaObject::invokeMethod(mainWindow,"ensureEditorFocus",Q_ARG(QVariant,QVariant::fromValue(target)));
                            noteFocusPrepared=true;return;
                        }
                        // Real wheel input follows a settled focus/layout frame;
                        // do not synthesize it in the same callback as focus change.
                        if(++noteFocusFrames<3)return;
                        if(flick->property("contentY").toDouble()>1
                            || notes->property("contentHeight").toDouble()>target->height()-20+1){fail("short notes focus must not scroll");return;}
                        if(!capture(QStringLiteral("scroll-short-notes-focus"))){fail("short notes focus capture");return;}
                    } else {
                        if(flick->property("contentY").toDouble()<=1){qCritical()<<"Wheel geometry"<<flick->property("contentY")<<flick->property("contentHeight")<<flick->property("height")<<qobject_cast<QQuickItem*>(notes)->mapToScene(QPointF(0,0))<<mainWindow->activeFocusItem();fail("notes wheel must scroll outer body");return;}
                        QMetaObject::invokeMethod(target,"forceActiveFocus");
                        if(target->property("wheelEnabled").toBool() || target->property("currentIndex").toInt()!=0){fail("combo wheel policy");return;}
                    }
                    const auto position=target->mapToScene(QPointF(target->width()/2,target->height()/2));
                    QWheelEvent wheel(position,mainWindow->mapToGlobal(position.toPoint()),QPoint(0,-96),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                    QCoreApplication::sendEvent(mainWindow,&wheel);
                    qInfo()<<"Synthetic wheel"<<phase<<wheel.isAccepted()<<mainWindow->isActive()<<flick->property("contentY")<<mainWindow->property("editorManualScroll");
                } else if(phase==150) {
                    auto* planning=mainWindow->findChild<QObject*>(QStringLiteral("editorPlanning"));
                    if(!planning || planning->property("currentIndex").toInt()!=0 || flick->property("contentY").toDouble()<=1){fail("wheel preserves choice and manual scroll");return;}
                    const auto policies=mainWindow->findChildren<QObject*>(QStringLiteral("daymarkScrollPhysics"));
                    int checked=0;
                    for(auto* policy:policies) {
                        auto* target=policy->property("target").value<QObject*>();if(!target)continue;++checked;
                        if(target->property("boundsBehavior").toInt()!=0 || target->property("maximumFlickVelocity").toDouble()!=1200
                           || target->property("flickDeceleration").toDouble()!=6000 || target->property("interactive").toBool()!=policy->property("overflowing").toBool()) {fail("shared scrolling policy");return;}
                    }
                    if(checked<8){fail("scrolling policy coverage");return;}
                    auto* shortList=mainWindow->findChild<QObject*>(QStringLiteral("taskList"));
                    if(!shortList || shortList->property("count").toInt()!=1 || shortList->property("contentHeight").toDouble()>shortList->property("height").toDouble()
                       || shortList->property("interactive").toBool()){fail("short task list must not scroll");return;}
                    for(auto* surface:mainWindow->findChildren<QQuickItem*>(QStringLiteral("daymarkScrollView"))) {
                        if(!surface->isVisible() || surface->width()<=0 || surface->height()<=0)continue;
                        auto* viewport=surface->property("contentItem").value<QObject*>();
                        if(!viewport || !viewport->inherits("QQuickFlickable") || viewport->property("contentHeight").toDouble()<=0){fail("scroll view retains automatic content sizing");return;}
                    }
                    if(!capture(QStringLiteral("scroll-short-notes"))){fail("short notes wheel capture");return;}
                    mainWindow->setProperty("draftDirty",false);QMetaObject::invokeMethod(editor,"close");m_workspace.setView(QStringLiteral("today"));
                } else if(phase==151) {
                    m_workspace.setTextScale(2);mainWindow->resize(880,580);
                    QMetaObject::invokeMethod(mainWindow,"openTool",Q_ARG(QVariant,QVariant(QStringLiteral("settings"))));
                    auto* settings=mainWindow->findChild<QObject*>(QStringLiteral("settingsDialog"));
                    if(!settings || !QMetaObject::invokeMethod(settings,"setVerificationCategory",Q_ARG(QVariant,QVariant(0)))){fail("categorized settings open");return;}
                } else if(phase>=152 && phase<=154) {
                    auto* settings=mainWindow->findChild<QObject*>(QStringLiteral("settingsDialog"));QVariant result;
                    if(!settings || settings->property("categoryIndex").toInt()!=phase-152 || !QMetaObject::invokeMethod(settings,"verifyCategory",Q_RETURN_ARG(QVariant,result)) || result.toString()!=QStringLiteral("PASS")){fail("settings category content and navigation");return;}
                    if(!capture(QStringLiteral("hig-settings-%1-200").arg(phase-152))){fail("settings category capture");return;}
                    if(phase<154) {
                        QVariant clicked;
                        if(!QMetaObject::invokeMethod(settings,"activateVerificationCategory",Q_RETURN_ARG(QVariant,clicked),Q_ARG(QVariant,QVariant(phase-151))) || !clicked.toBool()){fail("settings category click");return;}
                    } else {
                        QMetaObject::invokeMethod(settings,"close");m_workspace.setTextScale(1);mainWindow->resize(1120,720);
                        QMetaObject::invokeMethod(mainWindow,"openTool",Q_ARG(QVariant,QVariant(QStringLiteral("notifications"))));
                    }
                } else if(phase==155) {
                    auto* center=mainWindow->findChild<QObject*>(QStringLiteral("reminderCenter"));
                    if(!center || !center->property("visible").toBool() || !capture(QStringLiteral("hig-reminders"))){fail("reminder center");return;}
                    QMetaObject::invokeMethod(center,"close");
                    QMetaObject::invokeMethod(mainWindow,"openTool",Q_ARG(QVariant,QVariant(QStringLiteral("recurrence"))));
                    auto* title=mainWindow->findChild<QObject*>(QStringLiteral("recurrenceTitle"));
                    auto* create=mainWindow->findChild<QObject*>(QStringLiteral("newRecurrence"));
                    if(!title || !create){fail("rule navigation controls");return;}
                    title->setProperty("text",QStringLiteral("临时规则草稿"));QMetaObject::invokeMethod(title,"textEdited");QMetaObject::invokeMethod(create,"clicked");
                } else if(phase>=156 && phase<=158) {
                    auto* rule=mainWindow->findChild<QObject*>(QStringLiteral("recurrenceDialog"));
                    auto* title=mainWindow->findChild<QObject*>(QStringLiteral("recurrenceTitle"));
                    auto* discard=mainWindow->findChild<QObject*>(QStringLiteral("recurrenceDiscard"));
                    auto* create=mainWindow->findChild<QObject*>(QStringLiteral("newRecurrence"));
                    if(!rule || !title || !discard || !create){fail("rule draft objects");return;}
                    if(phase==156) {
                        if(!discard->property("visible").toBool() || title->property("text").toString()!=QStringLiteral("临时规则草稿")){fail("rule draft navigation guard");return;}
                        QMetaObject::invokeMethod(discard,"reject");
                    } else if(phase==157) {
                        if(!rule->property("dirty").toBool() || !rule->property("visible").toBool() || title->property("text").toString()!=QStringLiteral("临时规则草稿")){fail("rule cancel preserves draft");return;}
                        QMetaObject::invokeMethod(create,"clicked");QMetaObject::invokeMethod(discard,"accept");
                    } else {
                        if(rule->property("dirty").toBool() || !rule->property("visible").toBool() || !title->property("text").toString().isEmpty()){fail("rule discard continues requested navigation");return;}
                        QMetaObject::invokeMethod(rule,"close");m_workspace.setView(QStringLiteral("today"));
                    }
                } else if(phase==159) {
                    m_workspace.setThemeMode(2);
                    QMetaObject::invokeMethod(mainWindow,"openTool",Q_ARG(QVariant,QVariant(QStringLiteral("settings"))));
                    auto* settings=mainWindow->findChild<QObject*>(QStringLiteral("settingsDialog"));
                    if(!settings || !QMetaObject::invokeMethod(settings,"setVerificationCategory",Q_ARG(QVariant,QVariant(0)))){fail("default settings");return;}
                } else if(phase==160) {
                    auto* settings=mainWindow->findChild<QObject*>(QStringLiteral("settingsDialog"));QVariant result;
                    if(!settings || !QMetaObject::invokeMethod(settings,"verifyCategory",Q_RETURN_ARG(QVariant,result)) || result.toString()!=QStringLiteral("PASS") || !capture(QStringLiteral("hig-settings-default-dark"))){fail("default dark settings");return;}
                    QMetaObject::invokeMethod(settings,"close");m_workspace.setThemeMode(1);
                } else if(phase==161) {
                    m_workspace.setPreference(QStringLiteral("desktopCalendarOpacity"),70);
                    m_workspace.setPreference(QStringLiteral("desktopCalendarVisible"),true);
                } else if(phase==162) {
                    auto* calendar=mainWindow->findChild<QQuickWindow*>(QStringLiteral("desktopCalendarWindow"));QVariant independent;
                    if(!calendar || !calendar->isVisible() || !mainWindow->findChild<QQuickWindow*>(QStringLiteral("widgetWindow"))->isVisible() || !QMetaObject::invokeMethod(calendar,"verifyIndependence",Q_RETURN_ARG(QVariant,independent)) || !independent.toBool() || m_workspace.view()!=QStringLiteral("today")){fail("independent calendar and widget coexist");return;}
                    if(!screenshotPath.isEmpty() && !calendar->grabWindow().save(screenshotPath+QStringLiteral(".desktop-calendar.png"))){fail("desktop calendar capture");return;}
                    m_workspace.setTextScale(2);calendar->resize(880,640);
                } else if(phase==163) {
                    auto* calendar=mainWindow->findChild<QQuickWindow*>(QStringLiteral("desktopCalendarWindow"));
                    if(!calendar || !calendar->isVisible()){fail("large desktop calendar");return;}
                    if(!screenshotPath.isEmpty() && !calendar->grabWindow().save(screenshotPath+QStringLiteral(".desktop-calendar-200.png"))){fail("large desktop calendar capture");return;}
                    QMetaObject::invokeMethod(calendar,"close");
                } else if(phase==164) {
                    auto* calendar=mainWindow->findChild<QQuickWindow*>(QStringLiteral("desktopCalendarWindow"));
                    if(!calendar || calendar->isVisible() || !mainWindow->findChild<QQuickWindow*>(QStringLiteral("widgetWindow"))->isVisible() || m_workspace.preferences().value(QStringLiteral("desktopCalendarVisible")).toBool()){fail("calendar hide preserves task widget");return;}
                    m_workspace.setTextScale(1);
                } else if(phase==165) {
                    QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(QStringLiteral("event"))),Q_ARG(QVariant,QVariant(eventId)));
                    auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));
                    auto* frequency=panel->findChild<QObject*>(QStringLiteral("eventFrequency"));auto* alert=panel->findChild<QObject*>(QStringLiteral("eventAlert"));auto* save=panel->findChild<QObject*>(QStringLiteral("saveEvent"));
                    if(!frequency || !alert || !save){fail("event series controls");return;}
                    frequency->setProperty("currentIndex",2);alert->setProperty("currentIndex",2);QMetaObject::invokeMethod(save,"clicked");
                } else if(phase==166) {
                    const auto event=m_workspace.calendarEvent(eventId);if(event.value(QStringLiteral("repeatFrequency")).toString()!=QStringLiteral("WEEKLY") || event.value(QStringLiteral("alertMinutes")).toInt()!=5){fail("event series UI roundtrip");return;}
                    QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(QStringLiteral("event"))),Q_ARG(QVariant,QVariant(eventId)));
                } else if(phase==167 || phase==169) {
                    // Text-scale changes need rendered layout passes before reading
                    // nested form geometry, just like the editor viewport checks.
                    static int renderedSeriesPhase=-1;static int seriesFrames=0;
                    mainWindow->grabWindow();
                    if(renderedSeriesPhase!=phase){renderedSeriesPhase=phase;seriesFrames=0;return;}
                    if(++seriesFrames<3)return;
                    auto* dialog=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("eventDialog"));QVariant visible;
                    if(!QMetaObject::invokeMethod(dialog,"showSeriesForVerification",Q_RETURN_ARG(QVariant,visible)) || !visible.toBool()){fail("series group visible after layout");return;}
                } else if(phase==168) {
                    if(!capture(QStringLiteral("event-series"))){fail("series capture");return;}
                    m_workspace.setTextScale(2);m_workspace.setThemeMode(2);

                } else if(phase==170) {
                    if(!capture(QStringLiteral("event-series-dark-200"))){fail("large series capture");return;}
                    auto* dialog=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("eventDialog"));QMetaObject::invokeMethod(dialog,"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_workspace.setView(QStringLiteral("month"));mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->setProperty("showTasks",false);
                } else if(phase==171) {
                    if(!capture(QStringLiteral("calendar-event-bands"))){fail("calendar bands capture");return;}
                    mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->setProperty("showTasks",true);
                } else if(phase==172) {
                    const auto date=QDate::fromString(m_workspace.calendarEvent(eventId).value(QStringLiteral("startDate")).toString(),Qt::ISODate).addDays(7).toString(Qt::ISODate);
                    auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));QVariant jumped;
                    QMetaObject::invokeMethod(panel,"jumpToDate",Q_RETURN_ARG(QVariant,jumped),Q_ARG(QVariant,QVariant(date)));
                    QString key;for(const auto& day:m_workspace.calendarDaysFor(date,false))if(day.toMap().value(QStringLiteral("date")).toString()==date)for(const auto& event:day.toMap().value(QStringLiteral("events")).toList())if(event.toMap().value(QStringLiteral("eventId")).toString()==eventId)key=event.toMap().value(QStringLiteral("occurrenceKey")).toString();
                    if(!jumped.toBool() || key.isEmpty() || !QMetaObject::invokeMethod(panel,"inspectEvent",Q_ARG(QVariant,QVariant(eventId)),Q_ARG(QVariant,QVariant(key)))){fail("browse later occurrence");return;}
                } else if(phase==173) {
                    auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));auto* preview=panel->findChild<QObject*>(QStringLiteral("eventPreview"));
                    const auto date=QDate::fromString(m_workspace.calendarEvent(eventId).value(QStringLiteral("startDate")).toString(),Qt::ISODate).addDays(7).toString(Qt::ISODate);
                    if(!preview || !preview->property("visible").toBool() || preview->property("modal").toBool() || panel->findChild<QObject*>(QStringLiteral("eventDialog"))->property("visible").toBool() || !preview->findChild<QObject*>(QStringLiteral("previewDate"))->property("text").toString().contains(date) || !capture(QStringLiteral("event-preview"))){fail("read-only occurrence context");return;}
                    m_workspace.setTextScale(2);m_workspace.setThemeMode(2);
                } else if(phase==174) {
                    static int previewFrames=0;mainWindow->grabWindow();if(++previewFrames<4)return;
                    auto* preview=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("eventPreview"));auto* button=preview->findChild<QQuickItem*>(QStringLiteral("editPreviewEvent"));
                    if(!button || button->mapToScene(QPointF(0,button->height())).y()>mainWindow->height()+1 || !capture(QStringLiteral("event-preview-dark-200")) || !QMetaObject::invokeMethod(button,"clicked")){fail("preview edit action reachable");return;}
                } else if(phase==175) {
                    auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));auto* editor=panel->findChild<QObject*>(QStringLiteral("eventDialog"));
                    if(!editor->property("visible").toBool() || panel->findChild<QObject*>(QStringLiteral("eventPreview"))->property("visible").toBool()){fail("explicit series edit from preview");return;}
                    QMetaObject::invokeMethod(editor,"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);
                    QMetaObject::invokeMethod(panel->findChild<QObject*>(QStringLiteral("calendarJump")),"clicked");
                    panel->findChild<QObject*>(QStringLiteral("calendarJumpDate"))->setProperty("text",QStringLiteral("2026-02-30"));QMetaObject::invokeMethod(panel->findChild<QObject*>(QStringLiteral("confirmCalendarJump")),"clicked");
                } else if(phase==176) {
                    auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));
                    const auto expected=QDate::fromString(m_workspace.calendarEvent(eventId).value(QStringLiteral("startDate")).toString(),Qt::ISODate).addDays(7).toString(Qt::ISODate);
                    if(!panel->findChild<QObject*>(QStringLiteral("calendarJumpDialog"))->property("visible").toBool() || panel->property("selectedDate").toString()!=expected){fail("invalid jump preserves calendar");return;}
                    if(!capture(QStringLiteral("calendar-jump-error"))){fail("jump error capture");return;}
                    panel->findChild<QObject*>(QStringLiteral("calendarJumpDate"))->setProperty("text",QStringLiteral("2028-02-29"));QMetaObject::invokeMethod(panel->findChild<QObject*>(QStringLiteral("confirmCalendarJump")),"clicked");
                } else if(phase==177) {
                    auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));
                    if(panel->property("selectedDate").toString()!=QStringLiteral("2028-02-29") || panel->findChild<QObject*>(QStringLiteral("calendarJumpDialog"))->property("visible").toBool()){fail("leap day jump");return;}
                    m_workspace.setPreference(QStringLiteral("desktopCalendarVisible"),true);
                } else if(phase==178) {
                    auto* calendar=mainWindow->findChild<QQuickWindow*>(QStringLiteral("desktopCalendarWindow"));calendar->resize(420,480);
                    auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("desktopCalendarPanel"));QMetaObject::invokeMethod(panel,"openDateJump");
                    panel->findChild<QObject*>(QStringLiteral("calendarJumpDate"))->setProperty("text",QStringLiteral("2024-02-29"));QMetaObject::invokeMethod(panel->findChild<QObject*>(QStringLiteral("confirmCalendarJump")),"clicked");
                    m_workspace.setTextScale(2);QMetaObject::invokeMethod(panel,"inspectEvent",Q_ARG(QVariant,QVariant(eventId)),Q_ARG(QVariant,QVariant(QString())));
                } else if(phase==179) {
                    auto* calendar=mainWindow->findChild<QQuickWindow*>(QStringLiteral("desktopCalendarWindow"));static int narrowFrames=0;calendar->grabWindow();if(++narrowFrames<4)return;
                    auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));auto* independent=mainWindow->findChild<QObject*>(QStringLiteral("desktopCalendarPanel"));
                    auto* preview=independent->findChild<QObject*>(QStringLiteral("eventPreview"));auto* edit=preview->findChild<QQuickItem*>(QStringLiteral("editPreviewEvent"));
                    if(calendar->width()!=420 || calendar->height()!=480 || !preview->property("visible").toBool() || !edit || edit->mapToScene(QPointF(0,edit->height())).y()>calendar->height()+1 || (!screenshotPath.isEmpty() && !calendar->grabWindow().save(screenshotPath+QStringLiteral(".desktop-event-preview-200.png")))){fail("narrow large preview action");return;}
                    QMetaObject::invokeMethod(preview,"close");m_workspace.setTextScale(1);
                    if(!independent || independent->property("anchorDate").toString()!=QStringLiteral("2024-02-29") || panel->property("selectedDate").toString()!=QStringLiteral("2028-02-29")){fail("independent date jump");return;}
                    QMetaObject::invokeMethod(mainWindow->findChild<QQuickWindow*>(QStringLiteral("desktopCalendarWindow")),"close");
                    m_workspace.selectCalendarDate(m_workspace.calendarEvent(eventId).value(QStringLiteral("startDate")).toString());
                }
            } else if(phase>=112) {
                auto* widget=mainWindow->findChild<QQuickWindow*>(QStringLiteral("widgetWindow"));
                auto capture=[&](const QString& suffix){return screenshotPath.isEmpty() || mainWindow->grabWindow().save(screenshotPath+QLatin1Char('.')+suffix+QStringLiteral(".png"));};
                if(!widget){fail("refinement widget");return;}
                if(phase==112) {
                    QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(QStringLiteral("editor"))),Q_ARG(QVariant,QVariant(taskId)));
                } else if(phase==113) {
                    if(!QMetaObject::invokeMethod(startDate,"openPicker")){fail("date field open");return;}
                } else if(phase==114) {
                    const bool nativeFocus=QGuiApplication::platformName()==QStringLiteral("offscreen") || startDate->property("activeFocus").toBool();
                    if(!startDate->property("pickerVisible").toBool() || startDate->property("readOnly").toBool() || !nativeFocus || !capture(QStringLiteral("date-field"))){fail("editable date picker");return;}
                    QMetaObject::invokeMethod(startDate,"chooseDate",Q_ARG(QVariant,QVariant(m_workspace.dateOffset(1))));
                } else if(phase==115) {
                    if(startDate->property("pickerVisible").toBool() || startDate->property("text").toString()!=m_workspace.dateOffset(1) || !mainWindow->property("draftDirty").toBool()){fail("date selection draft");return;}
                    QMetaObject::invokeMethod(startDate,"chooseDate",Q_ARG(QVariant,QVariant(QString())));
                    if(!startDate->property("text").toString().isEmpty()){fail("date clear draft");return;}
                    if(QGuiApplication::platformName()!=QStringLiteral("offscreen")) {
                        QKeyEvent typed(QEvent::KeyPress,Qt::Key_unknown,Qt::NoModifier,QStringLiteral("2026-02-31"));QCoreApplication::sendEvent(mainWindow,&typed);
                    } else startDate->setProperty("text",QStringLiteral("2026-02-31"));
                    QMetaObject::invokeMethod(startDate,"openPicker");
                } else if(phase==116) {
                    if(startDate->property("text").toString()!=QStringLiteral("2026-02-31")){fail("picker retains manual input");return;}
                    QMetaObject::invokeMethod(startDate,"closePicker");startDate->setProperty("text",m_workspace.dateOffset(0));
                    if(!capture(QStringLiteral("refined-editor"))){fail("refined editor capture");return;}
                    mainWindow->setProperty("draftDirty",false);QMetaObject::invokeMethod(editor,"close");
                    m_workspace.setView(QStringLiteral("today"));
                } else if(phase==117) {
                    if(!capture(QStringLiteral("refined-today"))){fail("refined today capture");return;}
                    widget->resize(260,360);QMetaObject::invokeMethod(widget,"openVerificationMenu");
                } else if(phase==118 || phase==120) {
                    QVariant result;QMetaObject::invokeMethod(widget,"verifyMenu",Q_RETURN_ARG(QVariant,result));
                    if(result.toString()!=QStringLiteral("PASS")){qCritical()<<result;fail("widget menu labels fit");return;}
                    if(!screenshotPath.isEmpty()) {
                        bool captured=false;
                        for(auto* window:QGuiApplication::allWindows())if(auto* popup=qobject_cast<QQuickWindow*>(window);popup && popup!=mainWindow && popup!=widget && popup->isVisible()) {
                            captured=popup->grabWindow().save(screenshotPath+(phase==118 ? QStringLiteral(".widget-menu.png") : QStringLiteral(".widget-menu-200.png")));
                        }
                        if(!captured){fail("widget popup native capture");return;}
                    }
                    QMetaObject::invokeMethod(widget,"closeVerificationMenu");
                    if(phase==118)m_workspace.setTextScale(2);else m_workspace.setTextScale(1);
                } else if(phase==119)QMetaObject::invokeMethod(widget,"openVerificationMenu");
                else if(phase==121) {
                    mainWindow->setProperty("searchExpanded",true);mainWindow->setProperty("filtersExpanded",true);mainWindow->resize(880,580);m_workspace.setTextScale(2);
                    auto* search=mainWindow->findChild<QObject*>(QStringLiteral("taskSearch"));if(!search){fail("search entry");return;}search->setProperty("text",QStringLiteral("候选"));
                } else if(phase==122) {
                    if(!capture(QStringLiteral("refined-tools-200"))){fail("refined tools large capture");return;}
                    mainWindow->setProperty("searchExpanded",false);mainWindow->setProperty("filtersExpanded",false);m_workspace.setTextScale(1);mainWindow->resize(1120,720);
                    auto* search=mainWindow->findChild<QObject*>(QStringLiteral("taskSearch"));search->setProperty("text",QString());
                } else if(phase==123) {
                    m_workspace.setView(QStringLiteral("month"));
                    auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));
                    auto entry=m_workspace.task(taskId);entry.insert(QStringLiteral("kind"),QStringLiteral("both"));
                    if(!panel || !QMetaObject::invokeMethod(panel,"offerMove",Q_ARG(QVariant,QVariant(entry)),Q_ARG(QVariant,QVariant(m_workspace.dateOffset(0))))){fail("calendar move dialog open");return;}
                } else if(phase==124) {
                    auto* field=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("calendarMoveDate"));
                    if(!field || !QMetaObject::invokeMethod(field,"openPicker")){fail("calendar move date picker");return;}
                } else if(phase==125) {
                    auto* field=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("calendarMoveDate"));
                    auto* submit=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("confirmCalendarMove"));
                    if(!field || !field->property("pickerVisible").toBool() || !capture(QStringLiteral("calendar-move-picker")) || !submit){fail("calendar move date capture");return;}
                    QMetaObject::invokeMethod(field,"chooseDate",Q_ARG(QVariant,QVariant(m_workspace.dateOffset(1))));
                    QMetaObject::invokeMethod(submit,"clicked");
                } else if(phase==126) {
                    auto* dialog=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("calendarMoveDialog"));
                    const auto task=m_workspace.task(taskId);
                    if(!dialog || dialog->property("visible").toBool() || task.value(QStringLiteral("startOn")).toString()!=m_workspace.dateOffset(1) || task.value(QStringLiteral("deadlineOn")).toString()!=m_workspace.dateOffset(2)){fail("calendar move only changes selected field");return;}
                    m_workspace.setTextScale(2);mainWindow->resize(880,580);
                } else if(phase==127) {
                    auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));
                    auto entry=m_workspace.task(taskId);entry.insert(QStringLiteral("kind"),QStringLiteral("both"));
                    QMetaObject::invokeMethod(panel,"offerMove",Q_ARG(QVariant,QVariant(entry)),Q_ARG(QVariant,QVariant(m_workspace.dateOffset(1))));
                } else if(phase==128) {
                    auto* field=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("calendarMoveDate"));QMetaObject::invokeMethod(field,"openPicker");
                } else if(phase==129) {
                    if(!capture(QStringLiteral("calendar-move-picker-200"))){fail("large calendar date capture");return;}
                    auto* field=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("calendarMoveDate"));QMetaObject::invokeMethod(field,"closePicker");
                    auto* dialog=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("calendarMoveDialog"));QMetaObject::invokeMethod(dialog,"close");
                    m_workspace.setTextScale(1);mainWindow->resize(1120,720);m_workspace.setView(QStringLiteral("today"));
                }
            } else if(phase>=96) {
                auto capture=[&](const QString& suffix){return screenshotPath.isEmpty() || mainWindow->grabWindow().save(screenshotPath+QLatin1Char('.')+suffix+QStringLiteral(".png"));};
                auto* eventEditor=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("eventDialog"));
                auto show=[&](const QString& page,const QString& id){return QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(page)),Q_ARG(QVariant,QVariant(id)));};
                if(!eventEditor){fail("event editor");return;}
                if(phase==96) {
                    const auto choices=m_workspace.eventTaskChoices(QString());
                    for(const auto& value:choices){const auto task=value.toMap();if(task.value(QStringLiteral("taskStatus")).toString()==QStringLiteral("OPEN")){linkedTaskId=task.value(QStringLiteral("taskId")).toString();break;}}
                    if(linkedTaskId.isEmpty() || !m_workspace.saveEvent({{"title",QStringLiteral("产品评审（全天）")},{"allDay",true},{"startDate",m_workspace.dateOffset(0)},{"endDate",m_workspace.dateOffset(0)},{"links",QVariantList{QVariantMap{{"taskId",linkedTaskId},{"role",QStringLiteral("preparation")}}}}})){fail("create linked calendar event");return;}
                } else if(phase==97) {
                    if(m_workspace.calendarEvents().size()!=1){fail("event persisted");return;}eventId=m_workspace.calendarEvents().first().toMap().value(QStringLiteral("eventId")).toString();
                    const auto task=m_workspace.task(linkedTaskId);
                    if(!m_workspace.saveTask(linkedTaskId,task.value(QStringLiteral("rowVersion")).toLongLong(),task.value(QStringLiteral("title")).toString(),QString(),QStringLiteral("ACTIVE"),m_workspace.dateOffset(0),m_workspace.dateOffset(1),false)){fail("calendar agenda task");return;}
                } else if(phase==98) {
                    if(!m_workspace.saveEvent({{"title",QStringLiteral("设计讨论")},{"allDay",false},{"startDate",m_workspace.dateOffset(0)},{"endDate",m_workspace.dateOffset(0)},{"startTime",QStringLiteral("09:00")},{"endTime",QStringLiteral("10:00")},{"zone",m_workspace.systemTimeZone()}})){fail("create timed event");return;}
                } else if(phase==99) {if(m_workspace.calendarEvents().size()!=2 || !show(QStringLiteral("month"),QString())){fail("calendar events loaded");return;}mainWindow->resize(1000,720);}
                else if(phase==100){auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));QVariant titles;if(!panel || !QMetaObject::invokeMethod(panel,"verifyDayTitles",Q_RETURN_ARG(QVariant,titles)) || titles.toString()!=QStringLiteral("PASS")){qWarning()<<"Calendar title check:"<<titles;fail("calendar concrete titles");return;}if(!capture(QStringLiteral("calendar-agenda")) || !show(QStringLiteral("event"),eventId)){fail("event open");return;}}
                else if(phase==101){
                    auto* title=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("eventTitle"));auto* save=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("saveEvent"));
                    if(!eventEditor->property("visible").toBool() || !title || !save || !capture(QStringLiteral("calendar-event"))){fail("event form");return;}
                    title->setProperty("text",QStringLiteral("产品评审 · 已更新"));if(!QMetaObject::invokeMethod(save,"clicked")){fail("event form save");return;}
                } else if(phase==102){if(eventEditor->property("visible").toBool() || m_workspace.calendarEvent(eventId).value(QStringLiteral("title")).toString()!=QStringLiteral("产品评审 · 已更新")){fail("event UI committed");return;}}
                else if(phase==103){
                    const auto name=QStringLiteral("calendarComplete-")+linkedTaskId;
                    std::function<QQuickItem*(QQuickItem*)> findVisual=[&](QQuickItem* item)->QQuickItem*{if(!item)return nullptr;if(item->objectName()==name)return item;for(auto* child:item->childItems())if(auto* found=findVisual(child))return found;return nullptr;};
                    auto* complete=findVisual(mainWindow->contentItem());if(!complete || !QMetaObject::invokeMethod(complete,"clicked")){fail("calendar direct complete");return;}
                }
                else if(phase==104){if(m_workspace.task(linkedTaskId).value(QStringLiteral("taskStatus")).toString()!=QStringLiteral("COMPLETED") || m_workspace.calendarEvents().size()!=2){fail("calendar completion independent of event");return;}m_workspace.setCalendarCompleted(true);}
                else if(phase==105){if(!capture(QStringLiteral("calendar-completed"))){fail("completed agenda screenshot");return;}m_workspace.setTextScale(2);mainWindow->resize(880,580);}
                else if(phase==106){auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));QVariant titles;if(!panel || !QMetaObject::invokeMethod(panel,"verifyDayTitles",Q_RETURN_ARG(QVariant,titles)) || titles.toString()!=QStringLiteral("PASS")){qWarning()<<"Calendar title check:"<<titles;fail("compact calendar concrete titles");return;}if(!capture(QStringLiteral("calendar-200")) || !panel || !QMetaObject::invokeMethod(panel,"showAgenda")){fail("large calendar agenda entry");return;}}
                else if(phase==107){auto* agenda=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QQuickItem*>(QStringLiteral("calendarAgenda"));if(!agenda || agenda->mapToScene(QPointF(0,0)).y()<0 || agenda->mapToScene(QPointF(0,0)).y()>mainWindow->height() || !capture(QStringLiteral("calendar-agenda-200")) || !show(QStringLiteral("event"),eventId)){fail("large agenda reachable");return;}}
                else if(phase==108){if(!capture(QStringLiteral("calendar-event-200"))){fail("large event form");return;}eventEditor->setProperty("dirty",true);QMetaObject::invokeMethod(mainWindow,"close");}
                else if(phase==109){auto* discard=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QObject*>(QStringLiteral("eventDiscard"));if(!discard || !discard->property("visible").toBool()){fail("event draft close protection");return;}QMetaObject::invokeMethod(discard,"reject");}
                else if(phase==110){if(!eventEditor->property("visible").toBool() || !eventEditor->property("dirty").toBool()){fail("event draft cancellation retained");return;}eventEditor->setProperty("dirty",false);QMetaObject::invokeMethod(eventEditor,"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(2);mainWindow->resize(1440,900);}
                else if(phase==111){if(!capture(QStringLiteral("calendar-dark-wide"))){fail("dark calendar");return;}m_workspace.setThemeMode(1);mainWindow->resize(1120,720);}
            } else if(phase>=78) {
                auto* gallery=mainWindow->findChild<QObject*>(QStringLiteral("styleGallery"));
                if(!gallery){fail("style gallery");return;}
                auto state=[&](const QString& value){return QMetaObject::invokeMethod(gallery,"showState",Q_ARG(QVariant,QVariant(value)));};
                auto capture=[&](const QString& value){return screenshotPath.isEmpty() || mainWindow->grabWindow().save(screenshotPath+QLatin1Char('.')+value+QStringLiteral(".png"));};
                if(phase==78) {
                    QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(QStringLiteral("style"))),Q_ARG(QVariant,QVariant(taskId)));
                } else if(phase==79)state(QStringLiteral("focus"));
                else if(phase==80){if(!capture(QStringLiteral("ui-style")) || !state(QStringLiteral("menu"))){fail("style controls");return;}}
                else if(phase==81){if(!capture(QStringLiteral("ui-menu"))){fail("style menu");return;}QMetaObject::invokeMethod(gallery,"dismissPopups");state(QStringLiteral("choice"));}
                else if(phase==82){
                    if(!capture(QStringLiteral("ui-choice"))){fail("style choice");return;}
                    QVariant controlsOk;
                    if(!QMetaObject::invokeMethod(gallery,"verifyControls",Q_RETURN_ARG(QVariant,controlsOk)) || controlsOk.toString()!=QStringLiteral("PASS")){qWarning()<<"Style check:"<<controlsOk.toString();fail("style selection/edit/check interaction");return;}
                    QMetaObject::invokeMethod(gallery,"dismissPopups");m_workspace.setThemeMode(2);
                }
                else if(phase==83){if(!capture(QStringLiteral("ui-style-dark"))){fail("dark controls");return;}m_workspace.setTextScale(2);mainWindow->resize(880,580);}
                else if(phase==84){if(!capture(QStringLiteral("ui-style-200"))){fail("large controls");return;}m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(QStringLiteral("today"))),Q_ARG(QVariant,QVariant(taskId)));}
                else if(phase>=85 && phase<=88) {
                    const QStringList pages={QStringLiteral("organization"),QStringLiteral("recurrence"),QStringLiteral("data"),QStringLiteral("today")};
                    if(phase>85 && !capture(QStringLiteral("ui-")+pages[phase-86]+QStringLiteral("-200"))){fail("large management form");return;}
                    m_workspace.setTextScale(phase==88 ? 1 : 2);mainWindow->resize(phase==88 ? 1120 : 880,phase==88 ? 720 : 580);
                    QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(pages[phase-85])),Q_ARG(QVariant,QVariant(taskId)));
                }
                else if(phase>=89 && phase<=94) {
                    auto* widget=mainWindow->findChild<QQuickWindow*>(QStringLiteral("widgetWindow"));
                    if(!widget){fail("compact widget window");return;}
                    auto widgetCapture=[&](const QString& suffix){return screenshotPath.isEmpty() || widget->grabWindow().save(screenshotPath+QLatin1Char('.')+suffix+QStringLiteral(".png"));};
                    if(phase==89){widget->resize(260,360);m_workspace.setThemeMode(1);}
                    else if(phase==90){if(!widgetCapture(QStringLiteral("compact-widget"))){fail("compact widget screenshot");return;}m_workspace.setThemeMode(2);}
                    else if(phase==91){if(!widgetCapture(QStringLiteral("compact-widget-dark"))){fail("dark widget screenshot");return;}m_workspace.setTextScale(2);}
                    else if(phase==92){
                        auto* tasks=widget->findChild<QQuickItem*>(QStringLiteral("widgetTasks"));
                        auto* collapse=widget->findChild<QQuickItem*>(QStringLiteral("widgetCollapse"));
                        auto* pin=widget->findChild<QQuickItem*>(QStringLiteral("widgetPin"));
                        const bool wasPinned=m_widget.alwaysOnTop();
                        if(!pin || pin->width()<32 || pin->height()<32 || pin->mapToScene(QPointF(0,0)).x()<0
                           || pin->mapToScene(QPointF(pin->width(),pin->height())).x()>widget->width()
                           || pin->mapToScene(QPointF(pin->width(),pin->height())).y()>widget->height()
                           || pin->property("checked").toBool()!=wasPinned
                           || !QMetaObject::invokeMethod(pin,"clicked") || m_widget.alwaysOnTop()==wasPinned
                           || pin->property("checked").toBool()!=m_widget.alwaysOnTop()
                           || !QMetaObject::invokeMethod(pin,"clicked") || m_widget.alwaysOnTop()!=wasPinned){fail("floating pin toggle and state");return;}
                        if(!tasks || tasks->height()<120 || !collapse || collapse->height()<32 || collapse->height()>40 || collapse->mapToScene(QPointF(collapse->width(),0)).x()>widget->width() || !widgetCapture(QStringLiteral("compact-widget-200")) || !QMetaObject::invokeMethod(collapse,"clicked")){fail("compact large widget layout");return;}
                    } else if(phase==93){
                        auto* expand=widget->findChild<QQuickItem*>(QStringLiteral("widgetExpand"));
                        if(!m_widget.mini() || !expand || expand->height()<32 || expand->mapToScene(QPointF(0,expand->height())).y()>widget->height() || !widgetCapture(QStringLiteral("compact-mini-200")) || !QMetaObject::invokeMethod(expand,"clicked")){fail("compact mini expand");return;}
                    } else {if(m_widget.mini()){fail("compact widget restore");return;}m_workspace.setTextScale(1);m_workspace.setThemeMode(1);}
                }
            } else if(phase>=46) {
                mainWindow->grabWindow();
                auto* batch=mainWindow->findChild<QObject*>(QStringLiteral("batchTaskDialog"));
                auto* list=mainWindow->findChild<QObject*>(QStringLiteral("taskList"));
                auto invoke=[&](const char* method,const QString& value){return QMetaObject::invokeMethod(mainWindow,method,Q_ARG(QVariant,QVariant(value)));};
                auto capture=[&](const QString& suffix){return screenshotPath.isEmpty() || mainWindow->grabWindow().save(screenshotPath+QLatin1Char('.')+suffix+QStringLiteral(".png"));};
                if(!batch || !list){fail("candidate UI objects");return;}
                if(phase==46) {
                    QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(QStringLiteral("batch"))),Q_ARG(QVariant,QVariant(taskId)));
                } else if(phase==47) {
                    auto* apply=mainWindow->findChild<QObject*>(QStringLiteral("applyBatchSelection"));
                    if(!batch->property("visible").toBool() || !capture(QStringLiteral("v1-batch")) || !apply || !QMetaObject::invokeMethod(apply,"clicked")){fail("batch UI submit");return;}
                } else if(phase==48) {
                    if(batch->property("visible").toBool() || m_workspace.task(taskId).value(QStringLiteral("taskStatus")).toString()!=QStringLiteral("COMPLETED")){fail("batch UI commit");return;}
                } else if(phase>=49 && phase<=68) {
                    m_workspace.setView(QStringLiteral("today"));m_workspace.createTask(QStringLiteral("候选验收任务 %1").arg(phase-48));
                } else if(phase==69) {
                    list->setProperty("contentY",300);invoke("changeView",QStringLiteral("anytime"));
                } else if(phase==70) {
                    invoke("changeView",QStringLiteral("today"));
                } else if(phase==71) {
                    if(qAbs(list->property("contentY").toDouble()-300)>2 || !capture(QStringLiteral("v1-list"))){fail("per-view scroll restoration");return;}
                    m_workspace.setPreference(QStringLiteral("sidebarCollapsed"),true);m_workspace.setPreference(QStringLiteral("compact"),true);
                } else if(phase==72) {
                    if(mainWindow->property("sidebarWidth").toDouble()!=0 || !capture(QStringLiteral("v1-collapsed"))){fail("recoverable collapsed navigation");return;}
                    m_workspace.setPreference(QStringLiteral("sidebarCollapsed"),false);m_workspace.setTextScale(2);mainWindow->resize(880,580);invoke("openTool",QStringLiteral("settings"));
                } else if(phase==73) {
                    if(!capture(QStringLiteral("v1-settings-200"))){fail("candidate settings screenshot");return;}
                    QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(QStringLiteral("today"))),Q_ARG(QVariant,QVariant(taskId)));
                } else if(phase==74) {
                    auto* listItem=qobject_cast<QQuickItem*>(list);
                    if(!listItem || listItem->height()<96 || listItem->mapToScene(QPointF(0,listItem->height())).y()>mainWindow->height() || !capture(QStringLiteral("v1-list-200"))){fail("large text usable list");return;}
                    auto* navigation=mainWindow->findChild<QObject*>(QStringLiteral("navigationScroll"));
                    auto* tools=mainWindow->findChild<QQuickItem*>(QStringLiteral("settingsAndTools"));
                    auto* flick=navigation?qvariant_cast<QObject*>(navigation->property("contentItem")):nullptr;
                    if(!flick || !tools){fail("navigation scroll objects");return;}
                    flick->setProperty("contentY",qMax(0.0,flick->property("contentHeight").toDouble()-flick->property("height").toDouble()));
                    mainWindow->grabWindow();
                    if(tools->mapToScene(QPointF(0,tools->height())).y()>mainWindow->height() || !capture(QStringLiteral("v1-nav-200"))){fail("large text tools reachable");return;}
                    flick->setProperty("contentY",0);
                    m_workspace.setTextScale(1);mainWindow->resize(1120,720);m_workspace.setPreference(QStringLiteral("compact"),false);m_workspace.setThemeMode(2);
                } else if(phase==75) {
                    if(!capture(QStringLiteral("v1-dark"))){fail("candidate dark screenshot");return;}
                    m_workspace.setThemeMode(1);
                    auto* widget=mainWindow->findChild<QQuickWindow*>(QStringLiteral("widgetWindow"));
                    if(!widget || (!screenshotPath.isEmpty() && !widget->grabWindow().save(screenshotPath+QStringLiteral(".v1-widget.png"))) || !m_widget.setMini(true)){fail("native mini transition");return;}
                } else if(phase==76) {
                    auto* widget=mainWindow->findChild<QQuickWindow*>(QStringLiteral("widgetWindow"));
                    if(!widget || !m_widget.mini() || widget->height()<48 || widget->height()>100 || (!screenshotPath.isEmpty() && !widget->grabWindow().save(screenshotPath+QStringLiteral(".v1-mini.png"))) || !m_widget.setMini(false)){fail("mini rendering and restore");return;}
                }
            } else if(phase>=26) {
                // Hidden/minimized native windows need a render pass before layout assertions.
                const auto frame=mainWindow->grabWindow();
                auto invoke=[&](const char* method,const QString& value){return QMetaObject::invokeMethod(mainWindow,method,Q_ARG(QVariant,QVariant(value)));};
                auto click=[&](const char* name){auto* button=mainWindow->findChild<QObject*>(QLatin1String(name));return button && QMetaObject::invokeMethod(button,"clicked");};
                auto capture=[&](const QString& name){return screenshotPath.isEmpty() || frame.save(screenshotPath+QLatin1Char('.')+name+QStringLiteral(".png"));};
                auto* guard=mainWindow->findChild<QObject*>(QStringLiteral("unsavedChangesDialog"));
                auto* content=mainWindow->findChild<QQuickItem*>(QStringLiteral("taskContent"));
                const bool docked=mainWindow->property("detailDocked").toBool();
                if(!guard || !content){fail("responsive objects");return;}
                if(phase==26) {
                    mainWindow->resize(1120,720);m_workspace.setView(QStringLiteral("today"));m_workspace.createTask(QStringLiteral("第二项上下文"));
                } else if(phase==27) {
                    for(int i=0;i<m_workspace.tasks()->rowCount();++i){const auto id=m_workspace.tasks()->data(m_workspace.tasks()->index(i),TaskListModel::Id).toString();if(id!=taskId)secondId=id;}
                    if(secondId.isEmpty() || !invoke("openEditor",taskId)){fail("second task setup");return;}
                    editTitle->setProperty("text",QStringLiteral("待保留草稿"));
                } else if(phase==28) {
                    if(!docked || editor->property("modal").toBool() || content->width()<439.5 || !capture(QStringLiteral("docked-1120"))){qCritical()<<"Layout dimensions"<<mainWindow->size()<<content->width()<<docked<<editor->property("modal");fail("docked layout");return;}
                    mainWindow->resize(880,580);
                } else if(phase==29) {
                    if(docked || content->isVisible() || editTitle->property("text").toString()!=QStringLiteral("待保留草稿") || !capture(QStringLiteral("page-880"))){fail("narrow page preserves draft");return;}
                    mainWindow->resize(1440,900);
                } else if(phase==30) {
                    if(!docked || editTitle->property("text").toString()!=QStringLiteral("待保留草稿") || !capture(QStringLiteral("docked-1440")) || !invoke("openEditor",secondId)){fail("wide task navigation");return;}
                } else if(phase==31) {
                    if(!guard->property("visible").toBool() || !click("cancelLeave")){fail("cancel task switch");return;}
                } else if(phase==32) {
                    if(mainWindow->property("editId").toString()!=taskId || editTitle->property("text").toString()!=QStringLiteral("待保留草稿") || !invoke("openEditor",secondId)){fail("cancel retained task");return;}
                } else if(phase==33) {
                    if(!guard->property("visible").toBool() || !click("discardDraft")){fail("discard task switch");return;}
                } else if(phase==34) {
                    if(mainWindow->property("editId").toString()!=secondId || m_workspace.task(taskId).value(QStringLiteral("title")).toString()!=QStringLiteral("梳理今天的重点事项")){fail("discard zero mutation");return;}
                    editTitle->setProperty("text",QStringLiteral("保存后导航"));invoke("changeView",QStringLiteral("inbox"));
                } else if(phase==35) {
                    if(!guard->property("visible").toBool() || !click("saveDraftThenContinue")){fail("save before navigation");return;}
                } else if(phase==36) {
                    if(m_workspace.view()!=QStringLiteral("inbox") || editor->property("visible").toBool() || m_workspace.task(secondId).value(QStringLiteral("title")).toString()!=QStringLiteral("保存后导航")){fail("saved navigation");return;}
                    m_workspace.setView(QStringLiteral("today"));invoke("openEditor",secondId);editTitle->setProperty("text",QStringLiteral("完成前修改"));
                    QMetaObject::invokeMethod(mainWindow,"changeTaskState",Q_ARG(QVariant,QVariant(secondId)),Q_ARG(QVariant,QVariant(QStringLiteral("complete"))));
                } else if(phase==37) {
                    if(!guard->property("visible").toBool() || !click("saveDraftThenContinue")){fail("save before completion");return;}
                } else if(phase==38) {
                    const auto task=m_workspace.task(secondId);
                    if(task.value(QStringLiteral("taskStatus")).toString()!=QStringLiteral("COMPLETED") || task.value(QStringLiteral("title")).toString()!=QStringLiteral("完成前修改") || !m_workspace.errorText().isEmpty()){fail("completion fresh version");return;}
                    invoke("openEditor",taskId);editTitle->setProperty("text",QStringLiteral("导航将保留"));invoke("changeView",QStringLiteral("anytime"));
                } else if(phase==39) {
                    if(!guard->property("visible").toBool() || !click("cancelLeave")){fail("cancel view navigation");return;}
                } else if(phase==40) {
                    if(m_workspace.view()!=QStringLiteral("today") || editTitle->property("text").toString()!=QStringLiteral("导航将保留")){fail("navigation retained context");return;}
                    invoke("changeView",QStringLiteral("anytime"));
                } else if(phase==41) {
                    if(!click("discardDraft")){fail("discard view navigation");return;}
                } else if(phase==42) {
                    if(m_workspace.view()!=QStringLiteral("anytime") || editor->property("visible").toBool() || m_workspace.task(taskId).value(QStringLiteral("title")).toString()!=QStringLiteral("梳理今天的重点事项")){fail("discard navigation zero mutation");return;}
                    m_workspace.setTextScale(2.0);mainWindow->resize(1440,900);invoke("openEditor",taskId);
                } else if(phase==43) {
                    if(!docked || content->width()<439.5 || !capture(QStringLiteral("docked-200"))){qCritical()<<"Layout dimensions"<<mainWindow->size()<<content->width()<<docked;fail("large text docked");return;}
                    mainWindow->resize(880,580);
                } else if(phase==44) {
                    if(docked || content->isVisible() || !capture(QStringLiteral("page-200"))){fail("large text page");return;}
                    m_workspace.setTextScale(1.0);mainWindow->resize(1120,720);
                    QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(QStringLiteral("today"))),Q_ARG(QVariant,QVariant(taskId)));
                }
            }
            if(phase==179) {
                // Complete deferred control properties in the synthetic scene as
                // well as incubators; controller count alone does not cover them.
                QList<QPointer<QObject>> pending;
                for(auto* root:m_engine.rootObjects())pending.append(root);
                for(auto* child:m_engine.children())pending.append(child);
                for(auto* window:QGuiApplication::allWindows())pending.append(window);
                QSet<QObject*> visited;
                while(!pending.isEmpty()) {
                    auto* item=pending.takeLast().data();if(!item || visited.contains(item))continue;visited.insert(item);
                    qmlExecuteDeferred(item);
                    for(auto* child:item->children())pending.append(child);
                    if(auto* visual=qobject_cast<QQuickItem*>(item))for(auto* child:visual->childItems())pending.append(child);
                    if(auto* window=qobject_cast<QQuickWindow*>(item))pending.append(window->contentItem());
                }
            }
            ++phase;
            if(phase<180)return;
            // Occluded native windows may not receive frames to finish ListView
            // incubation. Complete the synthetic view before reporting/teardown.
            if(auto* controller=m_engine.incubationController();controller && controller->incubatingObjectCount()>0) {
                controller->incubateFor(5);
                // Let creation-triggered queued bindings settle before checking again.
                --phase;return;
            }
            int visibleWindows = 0;
            for (auto* window : QGuiApplication::allWindows()) {
                if (qobject_cast<QQuickWindow*>(window) && window->isVisible()
                    && window->width() > 0 && window->height() > 0)
                    ++visibleWindows;
            }
            const bool passed = visibleWindows == 2 && !m_qmlWarnings && m_workspace.ready();
            const bool screenshotOk=screenshotPath.isEmpty() || mainWindow->grabWindow().save(screenshotPath);
            qInfo() << "QML startup: visible windows=" << visibleWindows
                    << "warnings=" << m_qmlWarnings;
            *smokePhase=phase;
            if(passed && screenshotOk && soakSeconds>0) {
                probe->stop();
                auto* soak=new SoakRunner(m_workspace,m_widget,mainWindow,soakSeconds,soakReport,[this]{return m_qmlWarnings;},&m_engine);
                soak->start();return;
            }
            QCoreApplication::exit(passed && screenshotOk ? 0 : 2);
            probe->stop();
        });
        probe->start();
        // Capturing every surface adds synchronous GPU readbacks; keep the
        // ordinary/package probe at 45s and give screenshot runs a bounded 90s.
        QTimer::singleShot(screenshotPath.isEmpty() ? 45000 : 90000,&m_engine,[smokePhase]{if(*smokePhase>=180)return;qCritical()<<"UI smoke timeout at phase"<<*smokePhase;QCoreApplication::exit(3);});
    }
    return true;
}

} // namespace daymark

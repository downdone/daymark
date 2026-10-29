#include "AppRuntime.h"
#include "Ux34SmokeProbe.h"
#include "Ux35SmokeProbe.h"
#include "Ux37SmokeProbe.h"
#include "Ux38SmokeProbe.h"
#include "Ux39SmokeProbe.h"
#include "SoakRunner.h"
#include "sync/Coordinator.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QMessageBox>
#include <QQmlError>
#include <QQmlIncubator>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <QElapsedTimer>
#include <QDir>
#include <QStandardPaths>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QSet>
#include <QPointer>
#include <QIcon>
#include <atomic>
#ifdef Q_OS_MACOS
#include "platform/macos/SmokeActivation.h"
#endif

namespace daymark {
AppRuntime::AppRuntime()=default;
AppRuntime::~AppRuntime()=default;
namespace {
void activateSmokeWindow(QQuickWindow* window)
{
#ifdef Q_OS_MACOS
    if(QGuiApplication::platformName()==QStringLiteral("cocoa")) {
        activateMacSmokeWindow(window);
        return;
    }
#endif
    window->requestActivate();
}
}

bool AppRuntime::start(bool smokeTest, const QString& dataDirectory, const QString& screenshotPath,int soakSeconds,const QString& soakReport,const QString& smokeProfile)
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
    if (!m_widget.attachWindow(widgetWindow)) {
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
    m_menus=std::make_unique<ApplicationMenus>([window=QPointer<QQuickWindow>(mainWindow)](const QString& action){
        auto* focused=QGuiApplication::focusWindow();
        const QStringList calendarActions={"new","day","week","month","year","calendarToday","calendarNext","calendarPrevious","calendarGoTo","calendarEdit"};
        if(focused && focused->objectName()==QStringLiteral("desktopCalendarWindow") && calendarActions.contains(action)){QMetaObject::invokeMethod(focused,"calendarCommand",Q_ARG(QVariant,QVariant(action)));return;}
        if(window)QMetaObject::invokeMethod(window,"applicationCommand",Q_ARG(QVariant,QVariant(action)));
    });
    if(!smokeTest)m_tray=std::make_unique<DesktopTray>(m_widget,mainWindow);
    QObject::connect(&m_workspace,&TaskWorkspace::reminderReady,&m_engine,[this](const QString& title,const QString& message){
        if(m_tray)m_tray->notify(title,message);
    });
    QObject::connect(qApp,&QCoreApplication::aboutToQuit,&m_engine,[this]{m_widget.saveSettings();});
    QObject::connect(&m_instance,&SingleInstance::activationRequested,&m_engine,[mainWindow]{
        if(mainWindow) {mainWindow->showNormal();mainWindow->requestActivate();}
    });
    m_workspace.start(sync::ProfileIndex(directory).activePath());
    m_syncCredentials=std::make_unique<SystemSyncCredentials>();
    m_sync=std::make_unique<sync::Coordinator>(m_workspace,directory,*m_syncCredentials);
    // Restore geometry and load appearance before exposing either window. The
    // QML root stays hidden while the composition root prepares its first scene.
    QTimer::singleShot(0,&m_engine,[this,mainWindow,smokeTest,activate=!smokeProfile.isEmpty()]{
        if(!m_widget.setVisible(m_widget.initialVisible())){
            if(!smokeTest)QMessageBox::critical(nullptr,QObject::tr("Daymark 组件启动失败"),m_widget.errorText());
            QCoreApplication::exit(1);return;
        }
        mainWindow->setVisible(true);
        if(activate)activateSmokeWindow(mainWindow);
    });
    m_systemEvents=std::make_unique<SystemEventFilter>([this]{m_workspace.refreshClock();m_widget.normalizeGeometry();if(m_mainState)m_mainState->normalize();});
    QObject::connect(qGuiApp,&QGuiApplication::applicationStateChanged,&m_workspace,[this](Qt::ApplicationState state){if(state==Qt::ApplicationActive)m_systemEvents->requestRefresh();});

    if (smokeTest) {
        auto* probe=new QTimer(&m_engine);
        auto ux34=std::make_shared<Ux34SmokeProbe>(m_workspace,mainWindow,screenshotPath);
        auto ux35=std::make_shared<Ux35SmokeProbe>(m_workspace,mainWindow,screenshotPath);
        auto ux37=std::make_shared<Ux37SmokeProbe>(m_workspace,mainWindow,screenshotPath);
        auto ux38=std::make_shared<Ux38SmokeProbe>(m_workspace,m_widget,mainWindow,widgetWindow,screenshotPath);
        auto ux39=std::make_shared<Ux39SmokeProbe>(m_workspace,mainWindow,screenshotPath);
        auto smokePhase=std::make_shared<int>(0);
        auto smokeCompleted=std::make_shared<bool>(false);
        // Progress stamp for the stall guard. A loaded or software-rendered runner
        // legitimately spends far longer than 45s on 437 phases, so the guard must
        // bound the time without *progress*, not the total run time.
        auto smokeProgress=std::make_shared<std::atomic<qint64>>(-1);
        auto smokeProgressPhase=std::make_shared<int>(-1);
        auto inputClock=std::make_shared<QElapsedTimer>();
        inputClock->start();
        probe->setInterval(60);
        QObject::connect(probe,&QTimer::timeout,&m_engine,[this,probe,mainWindow,screenshotPath,ux34,ux35,ux37,ux38,ux39,smokeProfile,smokePhase,smokeCompleted,smokeProgress,smokeProgressPhase,inputClock,soakSeconds,soakReport,phase=smokeProfile.isEmpty()?0:436,taskId=QString(),secondId=QString(),eventId=QString(),linkedTaskId=QString(),detailProjectId=QString(),detailAreaId=QString(),detailHeadingId=QString(),quickOriginalNotes=QString(),quickTagId=QString(),quickProjectId=QString(),scheduleId=QString(),dragSource=QString(),dragTarget=QString(),dragStepFirst=QString(),dragPosition=QPointF(),inlineColumns=QVariantMap(),quickOriginalTask=QVariantMap(),completionRow=QPointer<QQuickItem>(),completionSeen=std::make_shared<std::atomic<bool>>(false),menuFrameMs=std::make_shared<std::atomic<qint64>>(-1),wheelFrames=std::make_shared<std::atomic<int>>(0),wheelConnection=std::make_shared<QMetaObject::Connection>()]() mutable {
            *smokePhase=phase;m_verificationPhase=phase;
            // A tick that keeps waiting on the same phase (busy worker, injected stall,
            // or a scene that never settles) is *no* progress: only a phase change
            // restamps the stall guard.
            if(phase!=*smokeProgressPhase){*smokeProgressPhase=phase;*smokeProgress=inputClock->elapsed();}
            bool stallConfigured=false;const int stall=qEnvironmentVariableIntValue("DAYMARK_SMOKE_STALL_PHASE",&stallConfigured);if(stallConfigured && phase==stall)return;
            if(m_workspace.busy())return;
            if(phase==100 || phase==106 || phase==241 || phase==405 || phase==429){
                auto* calendar=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));QVariant ready;
                mainWindow->grabWindow();
                if(calendar && QMetaObject::invokeMethod(calendar,"verifyDayTitles",Q_RETURN_ARG(QVariant,ready)) && ready.toString()==QStringLiteral("WAIT"))return;
            }
            auto fail=[probe](const char* stage){qCritical()<<"Basic UI smoke failed at"<<stage;probe->stop();QCoreApplication::exit(2);};
            if(!m_workspace.ready()) {fail("database initialization");return;}
            auto* input=mainWindow->findChild<QObject*>(QStringLiteral("newTaskTitle"));
            auto* editTitle=mainWindow->findChild<QObject*>(QStringLiteral("editTaskTitle"));
            auto* startDate=mainWindow->findChild<QObject*>(QStringLiteral("editStartDate"));
            auto* editor=mainWindow->findChild<QObject*>(QStringLiteral("taskEditor"));
            if(!input || !editTitle || !startDate || !editor){fail("UI objects");return;}
            if(phase==436){if(!smokeProfile.isEmpty()){const auto status=smokeProfile==QStringLiteral("calendar")?ux39->advance():smokeProfile==QStringLiteral("desktop")?ux38->advance():smokeProfile==QStringLiteral("selection")?ux37->advance():smokeProfile==QStringLiteral("continuous")?ux35->advance():ux34->advance();if(status.isEmpty())return;if(status!=QStringLiteral("DONE")){qCritical()<<status;fail("workspace scenario suite");return;}}} else if(phase==0) {
                const auto icon=QGuiApplication::windowIcon();
                if(icon.isNull() || icon.pixmap(16,16).isNull() || icon.pixmap(256,256).isNull()) {fail("application icon resources");return;}
                // Start-Process -WindowStyle Hidden suppresses the first Win32 show.
                // Native focus/layout checks require an exposed rendering surface.
                if(QGuiApplication::platformName()!=QStringLiteral("offscreen")) {
                    mainWindow->hide();mainWindow->show();activateSmokeWindow(mainWindow);
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
                startDate->setProperty("text",m_workspace.dateOffset(0));
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
                editTitle->setProperty("text",QString());
                mainWindow->close();
            } else if(phase==8) {
                auto* guard=mainWindow->findChild<QObject*>(QStringLiteral("unsavedChangesDialog"));
                auto* cancel=mainWindow->findChild<QObject*>(QStringLiteral("cancelLeave"));
                if(!guard || guard->property("visible").toBool() || !mainWindow->isVisible() || !editor->property("visible").toBool() || m_workspace.errorText().isEmpty() || !cancel) {fail("failed close autosave preserves task and window");return;}
                QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");
                if(!QMetaObject::invokeMethod(cancel,"clicked")) {fail("cancel leaving");return;}
            } else if(phase==9) {
                auto* guard=mainWindow->findChild<QObject*>(QStringLiteral("unsavedChangesDialog"));
                auto* back=mainWindow->findChild<QObject*>(QStringLiteral("returnFromEditor"));
                if(guard->property("visible").toBool() || !editor->property("visible").toBool() || !back) {fail("cancel preserves editor");return;}
                if(!QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor")) {fail("explicit discard with draft");return;}
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
            } else if(phase>=202 && phase<=271) {
                auto* quick=mainWindow->findChild<QObject*>(QStringLiteral("taskQuickEditor"));
                auto* reminder=mainWindow->findChild<QObject*>(QStringLiteral("editorReminder"));
                auto* guard=mainWindow->findChild<QObject*>(QStringLiteral("unsavedChangesDialog"));
                auto* event=mainWindow->findChild<QObject*>(QStringLiteral("taskEditor"));
                auto* fields=mainWindow->findChild<QObject*>(QStringLiteral("taskDetailsFields"));
                auto* creator=mainWindow->findChild<QObject*>(QStringLiteral("catalogCreateDialog"));
                auto findRow=[&](const QString& id){QVariant row;if(!QMetaObject::invokeMethod(mainWindow,"taskRowForVerification",Q_RETURN_ARG(QVariant,row),Q_ARG(QVariant,QVariant(id))))return static_cast<QQuickItem*>(nullptr);return qvariant_cast<QQuickItem*>(row);};
                auto find=[&](const char* name){return mainWindow->findChild<QObject*>(QString::fromLatin1(name));};
                auto click=[&](const char* name){auto* item=find(name);return item && QMetaObject::invokeMethod(item,"clicked");};
                auto capture=[&](const QString& name){return screenshotPath.isEmpty() || mainWindow->grabWindow().save(screenshotPath+QLatin1Char('.')+name+QStringLiteral(".png"));};
                if(!quick || !reminder || !event){fail("direct editing controls");return;}
                if(phase==202) {
                    quickOriginalTask=m_workspace.task(secondId);QMetaObject::invokeMethod(mainWindow,"openQuickEditor",Q_ARG(QVariant,QVariant(secondId)));}
                else if(phase==203) {
                    if(!quick->property("visible").toBool() || editor->property("visible").toBool()){qCritical()<<"Inline entry"<<quick->property("visible")<<editor->property("visible")<<mainWindow->property("inlineAttachAttempts")<<m_workspace.taskIndex(secondId);fail("quick entry separate from full editor");return;}
                    auto* title=quick->findChild<QObject*>(QStringLiteral("quickEditorTitle"));title->setProperty("text",QStringLiteral(""));QMetaObject::invokeMethod(title,"textEdited");
                    QMetaObject::invokeMethod(quick,"notesEdited",Q_ARG(QString,QStringLiteral("保持同一份备注草稿")));
                    QMetaObject::invokeMethod(quick->findChild<QObject*>(QStringLiteral("quickEditorStart")),"chooseDate",Q_ARG(QVariant,QVariant(m_workspace.dateOffset(1))));
                    QMetaObject::invokeMethod(quick,"closeRequested");
                } else if(phase==204) {
                    static bool cancelled=false;static int polished=0;
                    if(!cancelled){if(!quick->property("visible").toBool() || !mainWindow->property("draftDirty").toBool() || m_workspace.errorText().isEmpty() || guard->property("visible").toBool()){fail("failed inline autosave preserves draft without navigating");return;}QMetaObject::invokeMethod(quick,"titleEdited",Q_ARG(QString,QStringLiteral("快速编辑草稿")));cancelled=true;return;}
                    mainWindow->grabWindow();if(++polished<2)return;QVariant layout;
                    if(!quick->property("visible").toBool() || !QMetaObject::invokeMethod(quick,"verifyLayout",Q_RETURN_ARG(QVariant,layout)) || layout.toString()!=QStringLiteral("PASS") || !capture(QStringLiteral("quick-editor"))){qCritical()<<"Quick layout"<<layout<<quick->property("visible");capture(QStringLiteral("quick-layout-failure"));fail("quick cancel preserves usable card");return;}
                    click("quickEditorDetails");
                } else if(phase==205) {
                    if(quick->property("visible").toBool() || !editor->property("visible").toBool() || editTitle->property("text").toString()!=QStringLiteral("快速编辑草稿") || find("editTaskNotes")->property("text").toString()!=QStringLiteral("保持同一份备注草稿") || startDate->property("text").toString()!=m_workspace.dateOffset(1) || m_workspace.task(secondId).value(QStringLiteral("title")).toString()==QStringLiteral("快速编辑草稿")){fail("quick to full preserves one unsaved draft");return;}
                    QMetaObject::invokeMethod(startDate,"chooseDate",Q_ARG(QVariant,QVariant(QString())));
                    QMetaObject::invokeMethod(startDate,"chooseDate",Q_ARG(QVariant,QVariant(m_workspace.dateOffset(0))));QMetaObject::invokeMethod(startDate,"openPicker");
                } else if(phase==206) {
                    if(!startDate->property("pickerVisible").toBool() || !startDate->property("inlineExpanded").toBool() || startDate->property("text").toString()!=m_workspace.dateOffset(0) || !find("editDeadlineDate")->property("text").toString().isEmpty() || !capture(QStringLiteral("anchored-date"))){fail("date opens nearby calendar without changing deadline or expanding the form");return;}
                    QMetaObject::invokeMethod(startDate,"chooseDate",Q_ARG(QVariant,QVariant(m_workspace.dateOffset(2))));
                    QMetaObject::invokeMethod(reminder,"openPicker");
                } else if(phase==207) {
                    if(!reminder->property("pickerVisible").toBool() || startDate->property("pickerVisible").toBool()){fail("reminder direct picker");return;}
                    find("taskInspectorPicker")->setProperty("date",m_workspace.dateOffset(3));
                    auto* time=find("inspectorClock");QMetaObject::invokeMethod(time,"chooseTime",Q_ARG(QVariant,QVariant(14)),Q_ARG(QVariant,QVariant(35)));
                    click("applyInspectorPicker");
                    if(reminder->property("text").toString()!=m_workspace.dateOffset(3)+QStringLiteral(" 14:35")){fail("reminder date time draft");return;}
                    QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");
                } else if(phase==208) {
                    if(!guard->property("visible").toBool()){fail("full guard after quick transfer");return;}click("discardDraft");
                } else if(phase==209) {
                    const auto stored=m_workspace.task(secondId);
                    if(editor->property("visible").toBool() || stored!=quickOriginalTask){fail("discard whole quick and full draft zero mutation");return;}
                    QMetaObject::invokeMethod(mainWindow,"openQuickEditor",Q_ARG(QVariant,QVariant(secondId)));
                } else if(phase==210) {
                    auto* title=quick->findChild<QObject*>(QStringLiteral("quickEditorTitle"));title->setProperty("text",QStringLiteral("快速编辑已保存"));QMetaObject::invokeMethod(title,"textEdited");click("saveQuickEditor");
                } else if(phase==211) {
                    if(quick->property("visible").toBool() || editor->property("visible").toBool() || m_workspace.task(secondId).value(QStringLiteral("title")).toString()!=QStringLiteral("快速编辑已保存")){fail("quick atomic save");return;}
                    QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));reminder->setProperty("text",QStringLiteral("invalid"));QMetaObject::invokeMethod(reminder,"openPicker");
                } else if(phase==212) {
                    QMetaObject::invokeMethod(reminder,"closePicker");
                    if(reminder->property("text").toString()!=QStringLiteral("invalid")){fail("cancel picker preserves manual invalid input");return;}
                    mainWindow->setProperty("draftDirty",false);QMetaObject::invokeMethod(editor,"close");
                    auto* panel=find("calendarPanel");QMetaObject::invokeMethod(panel,"openEvent",Q_ARG(QVariant,QVariant(eventId)));
                } else if(phase==213) {
                    if(!editor->property("visible").toBool()){fail("calendar time uses task details");return;}auto* allDay=find("taskAllDay");if(!allDay->property("checked").toBool()){QMetaObject::invokeMethod(allDay,"toggle");QMetaObject::invokeMethod(allDay,"clicked");}auto* time=find("taskTimeStart");QMetaObject::invokeMethod(time,"chooseTime",Q_ARG(QVariant,QVariant(11)),Q_ARG(QVariant,QVariant(7)));if(time->property("text").toString()!=QStringLiteral("11:07") || !mainWindow->property("draftDirty").toBool()){fail("task time selection updates shared draft");return;}QMetaObject::invokeMethod(mainWindow,"revealTimingForVerification");QMetaObject::invokeMethod(time,"openPicker");
                } else if(phase==214) {
                    auto* time=find("taskTimeStart");if(!time->property("pickerVisible").toBool() || !capture(QStringLiteral("task-time-picker"))){fail("task time picker");return;}find("taskInspectorPicker")->findChild<QObject*>(QStringLiteral("pickerHour"))->setProperty("value",16);QKeyEvent cancelTime(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&cancelTime);if(time->property("pickerVisible").toBool() || !editor->property("visible").toBool()){fail("escape closes time picker and retains shared editor");return;}if(time->property("text").toString()!=QStringLiteral("11:07")){fail("cancel time picker zero draft mutation");return;}time->setProperty("text",QStringLiteral("25:90"));QMetaObject::invokeMethod(time,"openPicker");QMetaObject::invokeMethod(time,"closePicker");if(time->property("text").toString()!=QStringLiteral("25:90")){fail("invalid time remains available to correct");return;}QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");
                } else if(phase==215) {
                    if(!guard->property("visible").toBool() || !capture(QStringLiteral("discard-task-time-changes"))){fail("task time draft discard confirmation");return;}click("cancelLeave");if(!mainWindow->property("draftDirty").toBool() || !editor->property("visible").toBool()){fail("cancel retains time draft");return;}QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");click("discardDraft");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);mainWindow->resize(880,580);QMetaObject::invokeMethod(mainWindow,"openQuickEditor",Q_ARG(QVariant,QVariant(secondId)));
                } else if(phase==216) {
                    static int frames=0;mainWindow->grabWindow();if(++frames<3)return;QVariant layout;
                    auto* card=qobject_cast<QQuickItem*>(quick);auto* list=qobject_cast<QQuickItem*>(find("taskList"));const auto at=card->mapToItem(list,QPointF());
                    if(!QMetaObject::invokeMethod(quick,"verifyLayout",Q_RETURN_ARG(QVariant,layout)) || layout.toString()!=QStringLiteral("PASS") || at.y()<0 || at.y()+card->height()>list->height()+1 || !capture(QStringLiteral("quick-editor-dark-200"))){qCritical()<<"Quick viewport"<<layout<<at<<card->size()<<list->size();fail("quick 200 percent editing and actions stay in list viewport");return;}
                    QMetaObject::invokeMethod(quick,"closeRequested");
                } else if(phase==217) {
                    if(quick->property("visible").toBool() || event->property("visible").toBool() || mainWindow->property("draftDirty").toBool()){fail("clean quick close");return;}
                    m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);
                    QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));QMetaObject::invokeMethod(startDate,"chooseDate",Q_ARG(QVariant,QVariant(m_workspace.dateOffset(0))));QMetaObject::invokeMethod(startDate,"openShortcuts");
                } else if(phase==218) {
                    if(!startDate->property("pickerVisible").toBool() || startDate->property("text").toString()!=m_workspace.dateOffset(0) || !capture(QStringLiteral("direct-date-calendar"))){fail("calendar icon directly opens selected-date calendar");return;}
                    QMetaObject::invokeMethod(startDate,"chooseDate",Q_ARG(QVariant,QVariant(m_workspace.dateOffset(1))));
                    if(startDate->property("text").toString()!=m_workspace.dateOffset(1) || startDate->property("pickerVisible").toBool() || !find("editDeadlineDate")->property("text").toString().isEmpty()){fail("calendar changes only requested draft field");return;}
                    QMetaObject::invokeMethod(startDate,"chooseDate",Q_ARG(QVariant,QVariant(QString())));
                    if(!startDate->property("text").toString().isEmpty() || startDate->property("pickerVisible").toBool()){fail("clearing date collapses without changing deadline");return;}
                    mainWindow->setProperty("draftDirty",false);QMetaObject::invokeMethod(editor,"close");
                } else if(phase==219) {
                    m_workspace.setTextScale(2);m_workspace.setThemeMode(2);mainWindow->resize(880,580);
                    QMetaObject::invokeMethod(mainWindow,"openQuickEditor",Q_ARG(QVariant,QVariant(secondId)));quickOriginalNotes=m_workspace.task(secondId).value(QStringLiteral("notes")).toString();
                    QString longNotes;for(int line=0;line<60;++line)longNotes+=QStringLiteral("长备注键盘导航验收，正文保持一个滚动轴。\n");
                    QMetaObject::invokeMethod(quick,"notesEdited",Q_ARG(QString,longNotes));
                } else if(phase==220) {
                    static int frames=0;mainWindow->grabWindow();if(++frames<3)return;
                    auto* field=quick->findChild<QQuickItem*>(QStringLiteral("quickEditorStart"));field->forceActiveFocus();auto* notes=quick->findChild<QQuickItem*>(QStringLiteral("quickEditorNotes"));const auto point=notes->mapToScene(QPointF(notes->width()/2,notes->height()/2));QMouseEvent hover(QEvent::MouseMove,point,mainWindow->mapToGlobal(point),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&hover);
                } else if(phase==221) {
                    static int frames=0;mainWindow->grabWindow();if(++frames<3)return;
                    auto* field=quick->findChild<QQuickItem*>(QStringLiteral("quickEditorStart"));auto* scroll=mainWindow->findChild<QQuickItem*>(QStringLiteral("taskList"));const auto y=field->mapToItem(scroll,QPointF(0,0)).y();
                    if((QGuiApplication::platformName()!=QStringLiteral("offscreen") && !field->hasActiveFocus()) || y< -1 || y+field->height()>scroll->height()+1 || !capture(QStringLiteral("quick-long-notes-focus"))){capture(QStringLiteral("inline-focus-diagnostic"));qCritical()<<"Quick date focus"<<y<<field->height()<<scroll->height()<<scroll->property("contentY")<<scroll->property("contentHeight")<<mainWindow->property("quickFocusTarget");fail("inline summary keyboard reveals date in task list");return;}
                    auto* preview=quick->findChild<QObject*>(QStringLiteral("quickEditorNotes"));
                    if(!preview->property("readOnly").toBool() || preview->property("text").toString().contains(QLatin1Char('\n')) || find("editTaskNotes")->property("text").toString().count(QLatin1Char('\n'))!=60){fail("multiline summary retains complete draft");return;}
                    auto* tooltip=quick->findChild<QObject*>(QStringLiteral("quickNotesTooltip"));if(!tooltip->property("visible").toBool()){static int tooltipFrames=0;if(++tooltipFrames<10)return;fail("actual note hover opens full tooltip");return;}auto* fullText=quick->findChild<QObject*>(QStringLiteral("quickFullNotesText"));if(fullText->property("text").toString()!=find("editTaskNotes")->property("text").toString() || fullText->property("textFormat").toInt()!=int(Qt::PlainText) || tooltip->property("width").toDouble()>mainWindow->width()-31 || tooltip->property("height").toDouble()>mainWindow->height()*0.6 || !capture(QStringLiteral("full-notes-hover-dark-200"))){fail("full plain-text tooltip remains bounded and scrollable");return;}
                    QMetaObject::invokeMethod(quick,"closeRequested");
                } else if(phase==222) {
                    if(quick->property("visible").toBool() || mainWindow->property("draftDirty").toBool() || m_workspace.task(secondId).value(QStringLiteral("notes")).toString().count(QLatin1Char('\n'))!=60){fail("quick exit saves complete long notes");return;}
                    m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);
                } else if(phase==223) {
                    QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));QMetaObject::invokeMethod(creator,"showFor",Q_ARG(QVariant,QVariant(QStringLiteral("tag"))));
                    find("newCatalogName")->setProperty("text",QStringLiteral("就近标签验收"));click("createCatalogHere");
                } else if(phase==224) {
                    for(const auto& value:m_workspace.catalogOf(QStringLiteral("tag"))){const auto item=value.toMap();if(item.value("title").toString()==QStringLiteral("就近标签验收"))quickTagId=item.value("id").toString();}
                    if(quickTagId.isEmpty() || creator->property("visible").toBool() || !fields->property("tagIds").toStringList().contains(quickTagId) || m_workspace.task(secondId).value("tagIds").toStringList().contains(quickTagId)){fail("create tag selects draft without writing task");return;}
                    if(!capture(QStringLiteral("organize-with-created-tag"))){fail("organization header capture");return;}QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");click("discardDraft");
                } else if(phase==225) {
                    if(m_workspace.task(secondId).value("tagIds").toStringList().contains(quickTagId) || m_workspace.catalogOf(QStringLiteral("tag")).isEmpty()){fail("task discard keeps explicitly created library tag");return;}
                    QMetaObject::invokeMethod(mainWindow,"openQuickEditor",Q_ARG(QVariant,QVariant(secondId)));
                } else if(phase==226) {
                    static int polished=0;mainWindow->grabWindow();if(++polished<3)return;
                    auto* row=findRow(secondId);auto* visual=qobject_cast<QQuickItem*>(quick);bool descendant=false;for(auto* parent=visual->parentItem();parent;parent=parent->parentItem())if(parent==row)descendant=true;
                    auto* list=mainWindow->findChild<QQuickItem*>(QStringLiteral("taskList"));const auto cardY=visual->mapToItem(list,QPointF()).y();
                    if(!row || !row->property("expanded").toBool() || !descendant || visual->height()>180 || cardY< -1 || cardY+visual->height()>list->height()+1 || !capture(QStringLiteral("inline-original-row"))){qCritical()<<"Inline geometry"<<row<<descendant<<visual->height()<<cardY<<list->height()<<mainWindow->property("quickManualScroll");capture(QStringLiteral("inline-geometry-failure"));fail("quick editing is hosted and fully visible inside original row");return;}
                    QMetaObject::invokeMethod(creator,"showFor",Q_ARG(QVariant,QVariant(QStringLiteral("project"))));find("newCatalogName")->setProperty("text",QStringLiteral("就近分类验收"));click("createCatalogHere");
                } else if(phase==227) {
                    for(const auto& value:m_workspace.catalogOf(QStringLiteral("project"))){const auto item=value.toMap();if(item.value("title").toString()==QStringLiteral("就近分类验收"))quickProjectId=item.value("id").toString();}
                    if(quickProjectId.isEmpty() || fields->property("selectedDestination").toMap().value("id").toString()!=quickProjectId || !quick->property("visible").toBool() || m_workspace.task(secondId).value("projectId").toString()==quickProjectId){fail("create project keeps inline task draft and canonical selection");return;}
                    QMetaObject::invokeMethod(quick,"tagEdited",Q_ARG(QString,quickTagId),Q_ARG(bool,true));click("saveQuickEditor");
                } else if(phase==228) {
                    // Presentation latency is meaningful only for an exposed window.
                    // The long native probe can be occluded by the host application.
                    if(QGuiApplication::platformName()!=QStringLiteral("offscreen")){static int warmup=0;if(++warmup<3){mainWindow->showNormal();mainWindow->raise();mainWindow->requestActivate();mainWindow->update();return;}if(!mainWindow->isExposed()){fail("menu timing requires exposed native window");return;}}
                    const auto task=m_workspace.task(secondId);if(quick->property("visible").toBool() || task.value("projectId").toString()!=quickProjectId || !task.value("tagIds").toStringList().contains(quickTagId)){fail("inline organization atomic save");return;}
                    auto* row=findRow(secondId);if(!row){fail("menu target row");return;}
                    if(QGuiApplication::platformName()!=QStringLiteral("offscreen")){
                        auto started=std::make_shared<QElapsedTimer>();auto frames=std::make_shared<int>(0);auto connection=std::make_shared<QMetaObject::Connection>();
                        // A static menu legitimately needs only one frame. Drive
                        // two presentations explicitly instead of depending on
                        // an unrelated animation or a coalesced update request.
                        auto* presentationDriver=new QTimer(mainWindow);presentationDriver->setInterval(8);QObject::connect(presentationDriver,&QTimer::timeout,mainWindow,&QQuickWindow::update);presentationDriver->start();started->start();
                        // Timestamp on the emitting render thread; UI callbacks
                        // can be delayed by screenshot readback.
                        *connection=QObject::connect(mainWindow,&QQuickWindow::frameSwapped,&m_engine,[started,frames,connection,menuFrameMs,presentationDriver]{qInfo()<<"Menu presentation frame"<<*frames+1<<started->elapsed();if(++*frames<2)return;menuFrameMs->store(started->elapsed());QObject::disconnect(*connection);QMetaObject::invokeMethod(presentationDriver,[presentationDriver]{presentationDriver->stop();presentationDriver->deleteLater();},Qt::QueuedConnection);},Qt::DirectConnection);
                    }
                    QElapsedTimer command;command.start();QMetaObject::invokeMethod(row,"openVerificationMenu");qInfo()<<"Main menu open command ms:"<<command.nsecsElapsed()/1000000.0;mainWindow->update();
                } else if(phase==229) {
                    if(QGuiApplication::platformName()!=QStringLiteral("offscreen")){const auto elapsed=menuFrameMs->load();if(elapsed<0)return;qInfo()<<"Main menu two-presentation upper bound ms:"<<elapsed;if(elapsed>100){fail("menu presentation exceeded 100 ms");return;}}
                    auto* row=findRow(secondId);if(!row || !capture(QStringLiteral("same-window-task-menu"))){fail("menu scene capture");return;}
                    QMetaObject::invokeMethod(row,"closeVerificationMenu");completionRow=row;*completionSeen=false;
                    // Observe the actual row prepared by completeRow, then a
                    // presented animation frame. A 40ms GUI timer can run after
                    // the whole 190ms transition during synchronous readbacks.
                    auto tracked=std::make_shared<QPointer<QQuickItem>>(row);
                    auto candidate=std::make_shared<std::atomic<bool>>(false);
                    auto animationConnection=std::make_shared<QMetaObject::Connection>();
                    auto presentationConnection=std::make_shared<QMetaObject::Connection>();
                    *animationConnection=QObject::connect(mainWindow,&QQuickWindow::afterAnimating,probe,[tracked,candidate]{const auto current=tracked->data();candidate->store(current && current->isVisible() && current->height()>0 && current->opacity()>0 && current->property("removingAfterCompletion").toBool());});
                    *presentationConnection=QObject::connect(mainWindow,&QQuickWindow::frameSwapped,probe,[probe,mainWindow,screenshotPath,tracked,candidate,completionSeen,animationConnection,presentationConnection]{if(!candidate->load())return;completionSeen->store(true);QObject::disconnect(*presentationConnection);QMetaObject::invokeMethod(probe,[mainWindow,screenshotPath,tracked,animationConnection]{QObject::disconnect(*animationConnection);if(!screenshotPath.isEmpty() && *tracked && (*tracked)->property("removingAfterCompletion").toBool() && (*tracked)->opacity()>0)mainWindow->grabWindow().save(screenshotPath+QStringLiteral(".completion-feedback.png"));},Qt::QueuedConnection);},Qt::DirectConnection);
                    QMetaObject::invokeMethod(row,"completeRequested");
                    *tracked=qvariant_cast<QQuickItem*>(mainWindow->property("completionRow"));completionRow=*tracked;
                    if(!completionRow || completionRow->property("taskId").toString()!=secondId || !completionRow->property("completionFeedback").toBool()){fail("completion feedback tracks the prepared live row");return;}
                } else if(phase==230) {
                    if(m_workspace.task(secondId).value("taskStatus").toString()!=QStringLiteral("COMPLETED")){fail("complete commits before removal feedback");return;}
                } else if(phase==231) {
                    if(!*completionSeen){fail("completed row retains brief check feedback");return;}
                    m_workspace.setPreference(QStringLiteral("reduceMotion"),true);const auto task=m_workspace.task(secondId);m_workspace.changeState(secondId,task.value("rowVersion").toLongLong(),QStringLiteral("reopen"));
                } else if(phase==232) {
                    QMetaObject::invokeMethod(mainWindow,"openQuickEditor",Q_ARG(QVariant,QVariant(secondId)));QMetaObject::invokeMethod(quick,"closeRequested");
                } else if(phase==233) {
                    auto* row=findRow(secondId);if(!row || !row->property("reduceMotion").toBool()){fail("reduced motion reaches task row");return;}completionRow=row;QMetaObject::invokeMethod(row,"completeRequested");
                } else if(phase==234) {
                    if(m_workspace.task(secondId).value("taskStatus").toString()!=QStringLiteral("COMPLETED") || (completionRow && completionRow->property("removingAfterCompletion").toBool())){qCritical()<<"Completion probe state"<<m_workspace.task(secondId).value("taskStatus")<<(completionRow?completionRow->property("index"):QVariant())<<(completionRow?completionRow->property("removingAfterCompletion"):QVariant())<<m_workspace.taskIndex(secondId);capture(QStringLiteral("reduced-motion-failure"));fail("reduced motion removes without transition");return;}
                    m_workspace.setPreference(QStringLiteral("reduceMotion"),false);
                } else if(phase==235) {
                    const auto task=m_workspace.task(secondId);m_workspace.changeState(secondId,task.value("rowVersion").toLongLong(),QStringLiteral("reopen"));
                } else if(phase==236) {
                    quickOriginalTask=m_workspace.task(secondId);
                    QMetaObject::invokeMethod(mainWindow,"openQuickEditor",Q_ARG(QVariant,QVariant(secondId)));
                    QMetaObject::invokeMethod(quick->findChild<QObject*>(QStringLiteral("quickEditorReminder")),"openPicker");
                } else if(phase==237) {
                    auto* field=quick->findChild<QObject*>(QStringLiteral("quickEditorReminder"));auto* clock=field->findChild<QObject*>(QStringLiteral("reminderPickerTime"));
                    if(!field->property("pickerVisible").toBool() || clock->findChild<QObject*>(QStringLiteral("timePickerPopup"))){fail("reminder has one time selection layer");return;}
                    field->findChild<QObject*>(QStringLiteral("reminderPickerDate"))->setProperty("text",m_workspace.dateOffset(1));QMetaObject::invokeMethod(clock,"chooseTime",Q_ARG(QVariant,QVariant(16)),Q_ARG(QVariant,QVariant(20)));
                    if(!capture(QStringLiteral("reminder-single-level"))){fail("single reminder capture");return;}
                    QMetaObject::invokeMethod(field->findChild<QObject*>(QStringLiteral("applyReminder")),"clicked");
                    if(m_workspace.task(secondId)!=quickOriginalTask || !mainWindow->property("draftDirty").toBool()){fail("reminder selection edits only shared draft");return;}QMetaObject::invokeMethod(quick,"closeRequested");
                } else if(phase==238) {
                    const auto task=m_workspace.task(secondId);if(quick->property("editing").toBool() || mainWindow->property("draftDirty").toBool() || m_workspace.localReminder(task.value("remindAtUtc").toString())!=m_workspace.dateOffset(1)+QStringLiteral(" 16:20")){fail("one reminder confirmation commits task without another check");return;}
                    m_workspace.setView(QStringLiteral("month"));mainWindow->resize(1120,900);QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));
                } else if(phase==239) {
                    QVariant layout;QMetaObject::invokeMethod(mainWindow,"verifyTaskDetailLayout",Q_RETURN_ARG(QVariant,layout),Q_ARG(QVariant,QVariant(false)));
                    if(layout.toString()!=QStringLiteral("PASS") || !capture(QStringLiteral("ordered-details-light"))){fail("ordered scheduling rows");return;}
                    QMetaObject::invokeMethod(editor,"close");m_workspace.saveEvent({{"title",QStringLiteral("跨日假期验收")},{"startDate",QStringLiteral("2026-10-01")},{"endDate",QStringLiteral("2026-10-07")}});
                } else if(phase==240) {
                    m_workspace.selectCalendarDate(QStringLiteral("2026-10-01"));m_workspace.setView(QStringLiteral("month"));
                } else if(phase==241) {
                    mainWindow->grabWindow();auto* panel=find("calendarPanel");QVariant result;QMetaObject::invokeMethod(panel,"verifyDayTitles",Q_RETURN_ARG(QVariant,result));if(result.toString()!=QStringLiteral("PASS") || !capture(QStringLiteral("continuous-span-month"))){qCritical()<<"Calendar span geometry:"<<result;capture(QStringLiteral("continuous-span-failure"));fail("continuous task week span geometry");return;}QMetaObject::invokeMethod(mainWindow,"openTool",Q_ARG(QVariant,QVariant(QStringLiteral("recurrence"))));
                } else if(phase==242) {
                    if(m_workspace.view()!=QStringLiteral("repeating") || !find("taskList")->property("visible").toBool() || !capture(QStringLiteral("repeating-task-list"))){fail("repeat filter is an ordinary task list");return;}m_workspace.saveEvent({{"title",QStringLiteral("每年生日验收")},{"startDate",QStringLiteral("2026-10-07")},{"endDate",QStringLiteral("2026-10-07")},{"repeatFrequency",QStringLiteral("YEARLY")}});
                } else if(phase==243) {
                    for(const auto& value:m_workspace.calendarEvents())if(value.toMap().value("title")==QStringLiteral("每年生日验收"))scheduleId=value.toMap().value("taskId").toString();if(scheduleId.isEmpty() || !m_workspace.requestCalendarRange(QStringLiteral("2027-10-07"))){fail("request actual annual task instances for browsed calendar");return;}
                } else if(phase==244) {
                    QString id;for(const auto& day:m_workspace.calendarDaysFor(QStringLiteral("2027-10-07"),false))if(day.toMap().value("date").toString()==QStringLiteral("2027-10-07"))for(const auto& item:day.toMap().value("events").toList())if(item.toMap().value("title")==QStringLiteral("每年生日验收"))id=item.toMap().value("taskId").toString();if(id.isEmpty() || id==scheduleId){fail("future annual occurrence materialized once with own ID");return;}QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(id)));
                } else if(phase==245) {
                    if(!editor->property("visible").toBool() || !capture(QStringLiteral("annual-instance-shared-task"))){fail("annual task detail capture");return;}QMetaObject::invokeMethod(editor,"close");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);mainWindow->resize(880,580);QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));
                } else if(phase==246) {
                    if(!capture(QStringLiteral("ordered-details-dark-200"))){fail("dark ordered details capture");return;}QMetaObject::invokeMethod(editor,"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_workspace.setView(QStringLiteral("today"));mainWindow->resize(1120,720);
                } else if(phase==247) {
                    auto* list=find("taskList");list->setProperty("currentIndex",0);list->setProperty("contentY",list->property("originY"));
                    dragSource=m_workspace.tasks()->data(m_workspace.tasks()->index(0),TaskListModel::Id).toString();dragTarget=m_workspace.tasks()->data(m_workspace.tasks()->index(1),TaskListModel::Id).toString();
                } else if(phase==248) {
                    auto* row=findRow(dragSource);if(!row || !row->property("canReorder").toBool()){fail("drag source capability");return;}dragPosition=row->mapToScene(QPointF(row->width()*0.45,row->height()/2));
                    QMouseEvent press(QEvent::MouseButtonPress,dragPosition,mainWindow->mapToGlobal(dragPosition),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&press);inputClock->restart();
                } else if(phase==249) {
                    auto* row=findRow(dragSource);auto* target=findRow(dragTarget);if(row && !row->property("dragArmed").toBool() && inputClock->elapsed()<1000)return;
                    if(!row || !target || !row->property("dragArmed").toBool()){fail("long press arms drag without quick editing");return;}
                    dragPosition=target->mapToScene(QPointF(target->width()*0.45,target->height()/2));const auto intermediate=row->mapToScene(QPointF(row->width()*0.45,row->height()/2+16));QMouseEvent move(QEvent::MouseMove,intermediate,mainWindow->mapToGlobal(intermediate),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&move);
                } else if(phase==250) {
                    static bool moved=false;if(!moved){moved=true;QMouseEvent move(QEvent::MouseMove,dragPosition,mainWindow->mapToGlobal(dragPosition),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&move);return;}
                    auto* row=findRow(dragSource);if(!row || !row->property("dragging").toBool() || !capture(QStringLiteral("long-press-reorder"))){qCritical()<<"Drag lift state"<<row<<(row?row->property("dragArmed"):QVariant())<<(row?row->property("dragOriginY"):QVariant())<<dragPosition<<mainWindow->property("toolPage")<<find("taskList")->property("visible");if(row){auto* hold=row->findChild<QObject*>(QStringLiteral("taskHoldDrag"));qCritical()<<"Hold area"<<hold<<(hold?hold->property("pressed"):QVariant());}capture(QStringLiteral("drag-lift-failure"));fail("long press drag lift feedback");return;}
                    QMouseEvent release(QEvent::MouseButtonRelease,dragPosition,mainWindow->mapToGlobal(dragPosition),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&release);
                } else if(phase==251) {
                    if(m_workspace.taskIndex(dragSource)!=1 || mainWindow->property("draftDirty").toBool() || quick->property("editing").toBool()){fail("drag release reorders without opening editor");return;}m_workspace.reload();
                } else if(phase==252) {
                    if(m_workspace.taskIndex(dragSource)!=1){fail("drag order persists");return;}m_workspace.setSearch(QStringLiteral("候选"));
                } else if(phase==253) {
                    if(m_workspace.taskReorderingAllowed()){fail("filtered list disables reordering");return;}m_workspace.setSearch(QString());
                } else if(phase==254) {
                    static int settle=0;if(++settle==1){mainWindow->requestActivate();auto* list=find("taskList");list->setProperty("currentIndex",m_workspace.taskIndex(dragSource));QMetaObject::invokeMethod(list,"positionViewAtIndex",Q_ARG(int,m_workspace.taskIndex(dragSource)),Q_ARG(int,4));return;}if(settle<3)return;auto* row=findRow(dragSource);if(!row || !m_workspace.taskReorderingAllowed()){fail("cancel drag setup");return;}
                    dragPosition=row->mapToScene(QPointF(row->width()*0.45,row->height()/2));QMouseEvent press(QEvent::MouseButtonPress,dragPosition,mainWindow->mapToGlobal(dragPosition),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&press);inputClock->restart();
                } else if(phase==255) {
                    auto* row=findRow(dragSource);if(row && !row->property("dragArmed").toBool() && inputClock->elapsed()<1000)return;if(!row || !row->property("dragArmed").toBool()){qCritical()<<"Cancel hold state"<<row<<(row?row->mapToScene(QPointF(0,0)):QPointF())<<(row?row->property("canReorder"):QVariant())<<(row?row->property("heldPress"):QVariant())<<mainWindow->isActive()<<mainWindow->activeFocusItem();capture(QStringLiteral("cancel-drag-failure"));fail("cancel long press armed");return;}
                    dragPosition+=QPointF(0,16);QMouseEvent move(QEvent::MouseMove,dragPosition,mainWindow->mapToGlobal(dragPosition),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&move);
                } else if(phase==256) {
                    auto* row=findRow(dragSource);QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&escape);
                    if(!row || row->property("dragArmed").toBool() || row->property("dragging").toBool()){qCritical()<<"Escape focus"<<row<<mainWindow->activeFocusItem()<<QGuiApplication::focusWindow();fail("escape cancels active drag");return;}
                    QMouseEvent release(QEvent::MouseButtonRelease,dragPosition,mainWindow->mapToGlobal(dragPosition),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&release);
                    if(m_workspace.busy() || m_workspace.taskIndex(dragSource)!=1 || quick->property("editing").toBool()){fail("cancelled drag release has no write or edit");return;}
                } else if(phase==257) {
                    for(const char* name:{"sidebarTasksHeader","sidebarLibraryHeader","sidebarTagsHeader"})if(!click(name)){fail("collapsible navigation headers");return;}
                } else if(phase==258) {
                    if(m_workspace.preferences().value("sidebarTasksExpanded").toBool() || m_workspace.preferences().value("sidebarLibraryExpanded").toBool() || m_workspace.view()!=QStringLiteral("today") || !capture(QStringLiteral("collapsed-navigation"))){fail("independent collapsed sections preserve selection");return;}
                    click("sidebarLibraryHeader");
                } else if(phase==259) {
                    if(!m_workspace.preferences().value("sidebarLibraryExpanded").toBool() || m_workspace.preferences().value("sidebarTasksExpanded").toBool() || !capture(QStringLiteral("library-navigation"))){fail("library expands independently");return;}
                    click("sidebarTasksHeader");mainWindow->resize(1000,720);QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));
                    QMetaObject::invokeMethod(mainWindow,"selectEditorTab",Q_ARG(QVariant,QVariant(1)));auto* input=find("newSubtaskTitle");input->setProperty("text",QStringLiteral("独立子任务 A"));QMetaObject::invokeMethod(input,"accepted");
                } else if(phase==260) {
                    const auto children=m_workspace.subtaskChoices(secondId);if(children.size()!=1 || mainWindow->property("editorTab").toInt()!=1 || mainWindow->property("editId").toString()!=secondId || find("taskEditorScroll")->property("visible").toBool()){fail("subtask tab contains list without invisible competing detail");return;}dragStepFirst=children.first().toMap().value("taskId").toString();mainWindow->grabWindow();QVariant opened;if(!QMetaObject::invokeMethod(find("subtaskList"),"openForVerification",Q_RETURN_ARG(QVariant,opened),Q_ARG(QVariant,QVariant(dragStepFirst))) || !opened.toBool() || mainWindow->property("editId").toString()!=dragStepFirst){fail("actual child row opens own detail");return;}
                    editTitle->setProperty("text",QStringLiteral("子任务 A 已编辑"));click("saveTaskEditor");
                } else if(phase==261) {
                    if(!editor->property("visible").toBool() || mainWindow->property("editId").toString()!=secondId || mainWindow->property("editorTab").toInt()!=1 || m_workspace.task(dragStepFirst).value("title").toString()!=QStringLiteral("子任务 A 已编辑")){fail("child saves independently and returns parent list");return;}auto* input=find("newSubtaskTitle");input->setProperty("text",QStringLiteral("独立子任务 B"));QMetaObject::invokeMethod(input,"accepted");
                } else if(phase==262) {
                    if(m_workspace.subtaskChoices(secondId).size()!=2 || !capture(QStringLiteral("subtask-panel"))){fail("two real subtasks remain under parent");return;}mainWindow->grabWindow();QVariant target;if(!QMetaObject::invokeMethod(find("subtaskList"),"centerForVerification",Q_RETURN_ARG(QVariant,target),Q_ARG(QVariant,QVariant(dragStepFirst))) || target.toMap().isEmpty()){fail("visible child row pointer target");return;}const QPointF at(target.toMap().value("x").toDouble(),target.toMap().value("y").toDouble());QMouseEvent press(QEvent::MouseButtonPress,at,mainWindow->mapToGlobal(at),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&press);QMouseEvent release(QEvent::MouseButtonRelease,at,mainWindow->mapToGlobal(at),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&release);
                } else if(phase==263) {
                    if(mainWindow->property("editorTab").toInt()!=0 || !find("taskEditorScroll")->property("visible").toBool() || find("taskEditorTabs")->property("visible").toBool() || find("newSubtaskTitle")->property("visible").toBool() || !capture(QStringLiteral("leaf-subtask-details"))){fail("child detail visible with no child creation or nested tab");return;}
                    find("editTaskNotes")->setProperty("text",QStringLiteral("未保存子任务备注"));click("returnToParentSubtasks");
                    if(guard->property("visible").toBool() || !mainWindow->property("editorSaving").toBool()){fail("child return begins protected autosave");return;}
                } else if(phase==264) {
                    if(mainWindow->property("editId").toString()!=secondId || mainWindow->property("editorTab").toInt()!=1 || m_workspace.task(dragStepFirst).value("notes").toString()!=QStringLiteral("未保存子任务备注") || m_workspace.subtaskChoices(secondId).size()!=2){fail("discard child draft returns to intact parent list");return;}QMetaObject::invokeMethod(mainWindow,"selectEditorTab",Q_ARG(QVariant,QVariant(0)));
                    QMetaObject::invokeMethod(find("taskInfoToggle"),"triggered");
                } else if(phase==265) {
                    mainWindow->setProperty("editorManualScroll",true);auto* scroll=find("taskEditorScroll");scroll->setProperty("contentY",qMax(0.0,scroll->property("contentHeight").toDouble()-scroll->property("height").toDouble()));
                } else if(phase==266) {
                    if(!capture(QStringLiteral("detail-records-actions"))){fail("detail records screenshot");return;}QMetaObject::invokeMethod(find("taskEditHistoryDialog"),"close");reminder->setProperty("text",QStringLiteral("invalid"));click("saveTaskEditor");
                } else if(phase==267) {
                    if(!m_workspace.validationError() || !find("editorSaveError")->property("visible").toBool() || !editor->property("visible").toBool() || !capture(QStringLiteral("scoped-editor-error"))){fail("save validation is visible beside fixed actions");return;}click("returnFromEditor");
                } else if(phase==268) {
                    if(guard->property("visible").toBool() || !editor->property("visible").toBool() || !mainWindow->property("draftDirty").toBool()){fail("failed autosave leaves editor and draft intact");return;}QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");click("discardDraft");
                } else if(phase==269) {
                    if(editor->property("visible").toBool() || !m_workspace.errorText().isEmpty()){fail("discarded validation does not bleed into list");return;}
                    QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));startDate->setProperty("text",m_workspace.dateOffset(1));QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");if(!guard->property("visible").toBool()){fail("attention explicit discard guard");return;}click("discardDraft");QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));
                    const auto stored=m_workspace.task(secondId);
                    if(startDate->property("text")!=stored.value("startOn") || mainWindow->property("draftTodayFocus").toBool()!=stored.value("todayFocus").toBool()){fail("attention selection follows reloaded draft after discard");return;}
                    m_workspace.settingsFailed();QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));QMetaObject::invokeMethod(editor,"close");if(m_workspace.errorText().isEmpty() || m_workspace.validationError()){fail("persistence errors survive editor navigation");return;}m_workspace.clearError();
                    m_workspace.setTextScale(2);m_workspace.setThemeMode(2);mainWindow->resize(880,580);click("sidebarTasksHeader");click("sidebarLibraryHeader");click("sidebarTagsHeader");
                } else if(phase==270) {
                    if(!capture(QStringLiteral("collapsed-navigation-dark-200"))){fail("navigation dark large text");return;}
                    click("sidebarTasksHeader");click("sidebarLibraryHeader");click("sidebarTagsHeader");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);

                } else if(phase==271) {
                    if(find("sidebarPlanning")){fail("retired planning group remains in sidebar");return;}QMetaObject::invokeMethod(mainWindow,"openTool",Q_ARG(QVariant,QVariant(QStringLiteral("recurrence"))));
                }
                if(phase>=272 && phase<=308) {
                    // Window 272..308: own chain block keeps MSVC nesting under its limit.
                    // Phases 307/308 keep their original (skipped) siblings below.
                    auto verifyPage=[&]{mainWindow->grabWindow();QVariant result;const bool called=QMetaObject::invokeMethod(mainWindow,"verifyWorkspacePage",Q_RETURN_ARG(QVariant,result));if(!called || result.toString()!=QStringLiteral("PASS")){qCritical()<<"Workspace page:"<<result;capture(QStringLiteral("workspace-page-failure"));}return called && result.toString()==QStringLiteral("PASS");};
                    auto openPage=[&](const QString& name){QMetaObject::invokeMethod(mainWindow,"openTool",Q_ARG(QVariant,QVariant(name)));};
                    auto* ruleGuard=find("unsavedChangesDialog");
                    auto* library=find("organizationPage");auto* libraryTitle=find("organizationTitle");auto* libraryGuard=find("organizationDiscard");
                    if(phase==272) {
                        if(m_workspace.view()!=QStringLiteral("repeating") || !find("taskList")->property("visible").toBool() || !capture(QStringLiteral("repeating-filter-list"))){fail("loop tasks use normal list and row commands");return;}QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));auto* repeat=find("taskRepeating");QMetaObject::invokeMethod(repeat,"toggle");QMetaObject::invokeMethod(repeat,"clicked");QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");
                } else if(phase==273) {
                        if(!ruleGuard->property("visible").toBool() || !mainWindow->property("draftDirty").toBool()){fail("repeat draft guards list navigation");return;}click("cancelLeave");
                } else if(phase==274) {
                        if(!editor->property("visible").toBool() || !mainWindow->property("draftDirty").toBool()){fail("repeat task navigation cancellation retains editor");return;}QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");
                } else if(phase==275) {
                        if(!ruleGuard->property("visible").toBool()){fail("explicit discard guards task draft");return;}click("discardDraft");QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("month"))));
                } else if(phase==276) {
                        if(m_workspace.view()!=QStringLiteral("month") || !find("showCalendarView")->property("checked").toBool() || !capture(QStringLiteral("calendar-view-switch"))){fail("top calendar icon switches selected view");return;}openPage(QStringLiteral("organization"));
                } else if(phase==277) {
                        if(!verifyPage() || !capture(QStringLiteral("library-content-page"))){fail("library uses main content");return;}
                        QMetaObject::invokeMethod(library,"choose",Q_ARG(QVariant,m_workspace.categoryTree().first()));libraryTitle->setProperty("text",QStringLiteral("未保存的分类"));QMetaObject::invokeMethod(libraryTitle,"textEdited");openPage(QStringLiteral("notifications"));
                    } else if(phase==278) {
                        if(!libraryGuard->property("visible").toBool()){fail("library page navigation guard");return;}QMetaObject::invokeMethod(libraryGuard,"reject");
                    } else if(phase==279) {
                        if(mainWindow->property("toolPage").toString()!=QStringLiteral("organization") || libraryTitle->property("text").toString()!=QStringLiteral("未保存的分类")){fail("library cancel retains draft");return;}openPage(QStringLiteral("notifications"));
                    } else if(phase==280) {QMetaObject::invokeMethod(libraryGuard,"accept");}
                    else if(phase==281) {
                        if(mainWindow->property("toolPage").toString()!=QStringLiteral("notifications") || !verifyPage() || !capture(QStringLiteral("reminder-history-page"))){fail("reminder toolbar opens history page");return;}
                        for(const auto& item:m_workspace.catalogOf(QStringLiteral("project")))if(item.toMap().value("title").toString()==QStringLiteral("未保存的分类")){fail("discarded library draft was saved");return;}
                        click("returnFromWorkspacePage");click("sidebarLibraryCreate");
                    } else if(phase==282) {if(!find("catalogCreateDialog")->property("visible").toBool()){fail("category plus directly opens shared creator");return;}}
                    else if(phase==283) {
                        if(!find("catalogCreateDialog")->property("visible").toBool() || find("catalogCreateDialog")->property("selectAfterCreate").toBool()){fail("sidebar creates without task draft selection");return;}
                        find("newCatalogName")->setProperty("text",QStringLiteral("导航验收分类"));click("createCatalogHere");
                    } else if(phase==284) {
                        QString created;for(const auto& item:m_workspace.catalogOf(QStringLiteral("project")))if(item.toMap().value("title").toString()==QStringLiteral("导航验收分类"))created=item.toMap().value("id").toString();
                        if(created.isEmpty() || m_workspace.view()!=QStringLiteral("project:")+created || !mainWindow->property("toolPage").toString().isEmpty()){fail("created sidebar project is the actual destination");return;}
                        QMetaObject::invokeMethod(mainWindow,"editCurrentCatalog");
                    } else if(phase==285) {
                        if(!find("catalogEditPopup")->property("visible").toBool() || !mainWindow->property("toolPage").toString().isEmpty() || !capture(QStringLiteral("contextual-category-popup"))){fail("category context opens local editor");return;}
                        m_workspace.setTextScale(2);m_workspace.setThemeMode(2);mainWindow->resize(880,580);
                    } else if(phase==286) {
                        auto* popup=find("catalogEditPopup");if(!popup->property("visible").toBool() || popup->property("height").toDouble()>mainWindow->height()-31 || !capture(QStringLiteral("catalog-popup-dark-200"))){fail("catalog popup narrow large text");return;}QMetaObject::invokeMethod(popup,"close");QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));QMetaObject::invokeMethod(mainWindow,"revealTimingForVerification");
                } else if(phase==287) {
                        if(!editor->property("visible").toBool() || !capture(QStringLiteral("task-repeat-controls-dark-200"))){fail("repeat controls in shared detail at 200 percent");return;}QMetaObject::invokeMethod(editor,"close");openPage(QStringLiteral("notifications"));
                } else if(phase==288) {
                        if(!verifyPage() || !capture(QStringLiteral("reminder-page-dark-200"))){fail("history page large text");return;}m_workspace.setPreference(QStringLiteral("sidebarCollapsed"),true);
                    } else if(phase==289) {
                        if(!verifyPage() || !find("toggleSidebar")->property("visible").toBool()){fail("page retains sidebar recovery");return;}click("toggleSidebar");click("returnFromWorkspacePage");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);
                    } else if(phase==290) {
                        QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));editTitle->setProperty("text",QStringLiteral("页面导航前的任务草稿"));QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");
                    } else if(phase==291) {
                        if(!guard->property("visible").toBool()){fail("task draft protects page navigation");return;}click("cancelLeave");
                    } else if(phase==292) {
                        if(!mainWindow->property("toolPage").toString().isEmpty() || editTitle->property("text").toString()!=QStringLiteral("页面导航前的任务草稿")){fail("task navigation cancel keeps context");return;}QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");click("discardDraft");openPage(QStringLiteral("organization"));
                    } else if(phase==293) {
                        if(!verifyPage() || m_workspace.task(secondId).value("title").toString()==QStringLiteral("页面导航前的任务草稿")){fail("task discard opens page without saving");return;}click("returnFromWorkspacePage");
                    } else if(phase==294) {
                        if(!mainWindow->property("toolPage").toString().isEmpty() || mainWindow->property("toolModalOpen").toBool()){fail("pages return to unobstructed content");return;}
                        QMetaObject::invokeMethod(mainWindow,"manageCatalog",Q_ARG(QVariant,QVariant(QStringLiteral("tag"))));
                    } else if(phase==295) {
                        if(library->property("filterKind").toString()!=QStringLiteral("tag") || !verifyPage() || !capture(QStringLiteral("tag-management-page"))){fail("tags have their own management context");return;}
                        QMetaObject::invokeMethod(library,"choose",Q_ARG(QVariant,m_workspace.catalogOf(QStringLiteral("tag")).first()));libraryTitle->setProperty("text",QStringLiteral("标签草稿"));QMetaObject::invokeMethod(libraryTitle,"textEdited");
                        QMetaObject::invokeMethod(find("catalogPageTabs"),"activateForVerification",Q_ARG(QVariant,QVariant(0)));
                    } else if(phase==296) {
                        if(!libraryGuard->property("visible").toBool()){fail("concept tabs protect the current draft");return;}QMetaObject::invokeMethod(libraryGuard,"reject");
                    } else if(phase==297) {
                        if(library->property("filterKind").toString()!=QStringLiteral("tag") || libraryTitle->property("text").toString()!=QStringLiteral("标签草稿")){fail("cancel concept switch retains type and draft");return;}
                        QMetaObject::invokeMethod(find("catalogPageTabs"),"activateForVerification",Q_ARG(QVariant,QVariant(0)));QMetaObject::invokeMethod(libraryGuard,"accept");
                    } else if(phase==298) {
                        if(library->property("filterKind").toString()!=QStringLiteral("project") || library->property("editing").toBool() || !verifyPage()){fail("discard switches concepts without a blank generic form");return;}
                        click("returnFromWorkspacePage");click("sidebarTagsCreate");
                    } else if(phase==299) {
                        auto* dialog=find("catalogCreateDialog");if(!dialog->property("visible").toBool() || dialog->property("kind").toString()!=QStringLiteral("tag") || dialog->property("selectAfterCreate").toBool()){fail("tag sidebar creates a cross-cutting label");return;}QMetaObject::invokeMethod(dialog,"close");
                    } else if(phase==300) {
                        if(!m_workspace.saveCatalog({{"kind",QStringLiteral("area")},{"title",QStringLiteral("工作（示例）")}})){fail("philosophy example area");return;}
                    } else if(phase==301) {
                        QString area;for(const auto& value:m_workspace.catalogOf(QStringLiteral("area")))if(value.toMap().value("title").toString()==QStringLiteral("工作（示例）"))area=value.toMap().value("id").toString();
                        if(area.isEmpty() || !m_workspace.saveCatalog({{"kind",QStringLiteral("project")},{"title",QStringLiteral("发布 Daymark 1.0（示例）")},{"parentId",area}})){fail("project belongs to a responsibility");return;}
                    } else if(phase==302) {if(!m_workspace.saveCatalog({{"kind",QStringLiteral("tag")},{"title",QStringLiteral("电话（示例）")}})){fail("philosophy cross-cutting tag");return;}}
                    else if(phase==303) {
                        m_workspace.setPreference(QStringLiteral("sidebarTasksExpanded"),false);m_workspace.setPreference(QStringLiteral("sidebarPlanningExpanded"),false);m_workspace.setPreference(QStringLiteral("sidebarLibraryExpanded"),true);m_workspace.setPreference(QStringLiteral("sidebarTagsExpanded"),true);
                    } else if(phase==304) {
                        bool nested=false;for(const auto& entry:m_workspace.navigationCatalog())if(entry.toMap().value("title").toString()==QStringLiteral("发布 Daymark 1.0（示例）"))nested=entry.toMap().value("depth").toInt()==1;
                        if(!nested || !capture(QStringLiteral("ownership-and-tags"))){fail("sidebar shows responsibility-result hierarchy separately from labels");return;}
                        m_workspace.setPreference(QStringLiteral("sidebarTasksExpanded"),true);m_workspace.setPreference(QStringLiteral("sidebarPlanningExpanded"),true);
                    } else if(phase==305) {
                        openPage(QStringLiteral("schedules"));
                    } else if(phase==306) {
                        if(m_workspace.view()!=QStringLiteral("repeating") || m_workspace.tasks()->rowCount()<1){fail("repeating task filter has actual instances");return;}const auto id=m_workspace.tasks()->data(m_workspace.tasks()->index(0),TaskListModel::Id).toString();QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(id)));if(!editor->property("visible").toBool() || !find("taskRepeating")->property("checked").toBool()){fail("repeat list opens the ordinary task editor");return;}QMetaObject::invokeMethod(editor,"close");
                } else if(phase==307) {
                        QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("inbox"))));
                }
                    else if(phase==308) {
                        if(!mainWindow->property("toolPage").toString().isEmpty() || mainWindow->property("toolModalOpen").toBool()){fail("concept pages finish without hidden modals");return;}
                    }

                } else if(phase>=309 && phase<=332) {
                    auto* deadline=find("editDeadlineDate");auto* picker=find("taskDatePicker");auto* panel=find("calendarPanel");
                    auto movePointer=[&](QEvent::Type type,const QPointF& point){QMouseEvent event(type,point,mainWindow->mapToGlobal(point),type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&event);};
                    auto startDrag=[&]{auto* list=find("taskList");list->setProperty("currentIndex",m_workspace.taskIndex(dragSource));QMetaObject::invokeMethod(list,"forceLayout");QMetaObject::invokeMethod(list,"positionViewAtIndex",Q_ARG(int,m_workspace.taskIndex(dragSource)),Q_ARG(int,4));mainWindow->grabWindow();auto* row=findRow(dragSource);if(!row || !row->property("canDrag").toBool()){qCritical()<<"Navigation drag row"<<phase<<row<<m_workspace.taskIndex(dragSource)<<m_workspace.view();return false;}row->forceActiveFocus();mainWindow->grabWindow();auto* handle=row->findChild<QQuickItem*>(QStringLiteral("taskDragHandle"));if(!handle)return false;auto* viewport=qobject_cast<QQuickItem*>(list);auto inList=handle->mapToItem(viewport,QPointF(handle->width()/2,handle->height()/2));if(!viewport->contains(inList)){const auto origin=list->property("originY").toDouble(),maximum=origin+qMax(0.0,list->property("contentHeight").toDouble()-viewport->height());list->setProperty("contentY",qBound(origin,list->property("contentY").toDouble()+inList.y()-viewport->height()/2,maximum));mainWindow->grabWindow();inList=handle->mapToItem(viewport,QPointF(handle->width()/2,handle->height()/2));}if(!viewport->contains(inList)){qCritical()<<"Navigation drag clip"<<phase<<inList<<viewport->size()<<list->property("contentY")<<list->property("contentHeight");return false;}dragPosition=handle->mapToScene(QPointF(handle->width()/2,handle->height()/2));movePointer(QEvent::MouseButtonPress,dragPosition);if(!row->property("dragArmed").toBool())qCritical()<<"Navigation drag press"<<row<<row->isVisible()<<row->property("canDrag")<<handle->isVisible()<<handle->mapToScene(QPointF())<<mainWindow->property("editingActive");return row->property("dragArmed").toBool();};
                    std::function<QQuickItem*(QQuickItem*,const QString&)> visual=[&](QQuickItem* item,const QString& name)->QQuickItem*{if(item->objectName()==name)return item;for(auto* child:item->childItems())if(auto* result=visual(child,name))return result;return nullptr;};
                    auto targetPoint=[&](const char* name){auto* target=visual(mainWindow->contentItem(),QString::fromLatin1(name));if(!target){fail("navigation destination missing from visual tree");return QPointF();}return target->mapToScene(QPointF(target->width()/2,target->height()/2));};
                    auto positionTask=[&]{QMetaObject::invokeMethod(mainWindow,"revealTaskForVerification",Q_ARG(QVariant,QVariant(dragSource)));};
                    if(phase==309){m_workspace.setView(QStringLiteral("inbox"));m_workspace.createTask(QStringLiteral("直接操作与跨天范围"));}
                    else if(phase==310){dragSource.clear();for(const auto& task:m_workspace.tasks()->items())if(task.title==QStringLiteral("直接操作与跨天范围"))dragSource=task.id;if(dragSource.isEmpty()){fail("direct action task");return;}positionTask();}
                    else if(phase==311){
                        auto* row=findRow(dragSource);auto* details=row?row->findChild<QQuickItem*>(QStringLiteral("taskDetails-")+dragSource):nullptr;
                        if(!details){fail("direct detail button missing");return;}const auto point=details->mapToScene(QPointF(details->width()/2,details->height()/2));movePointer(QEvent::MouseButtonPress,point);movePointer(QEvent::MouseButtonRelease,point);
                        if(!editor->property("visible").toBool() || quick->property("editing").toBool()){fail("row button directly opens complete details");return;}
                        startDate->setProperty("text",m_workspace.dateOffset(0));deadline->setProperty("text",m_workspace.dateOffset(5));
                        if(mainWindow->property("draftPlanningState").toString()!=QStringLiteral("ACTIVE")){fail("dates activate legacy inbox draft");return;}click("saveTaskEditor");
                    } else if(phase==312){
                        const auto task=m_workspace.task(dragSource);if(task.value("startOn").toString()!=m_workspace.dateOffset(0) || task.value("deadlineOn").toString()!=m_workspace.dateOffset(5)){fail("date draft persists");return;}
                        m_workspace.setView(QStringLiteral("all"));positionTask();
                    } else if(phase==313){if(m_workspace.tasks()->rowCount()<1 || m_workspace.view()!=QStringLiteral("all")){fail("all view keeps dated unclassified task");return;}positionTask();}
                    else if(phase==314){auto* row=findRow(dragSource);if(!row || !row->property("canReorder").toBool() || !startDrag()){fail("all view task can reorder and drag to smart view");return;}}
                    else if(phase==315){dragPosition=targetPoint("navigation-today");movePointer(QEvent::MouseMove,dragPosition);}
                    else if(phase==316){auto* destination=visual(mainWindow->contentItem(),QStringLiteral("navigation-todayDropTarget"));if(!destination || !destination->property("containsDrag").toBool() || !destination->parentItem()->property("taskDropActive").toBool() || !capture(QStringLiteral("task-navigation-drop"))){fail("sidebar highlights actual task drag");return;}movePointer(QEvent::MouseButtonRelease,dragPosition);}
                    else if(phase==317){const auto task=m_workspace.task(dragSource);if(task.value("planningState")!=QStringLiteral("ACTIVE") || !task.value("todayFocus").toBool()){fail("sidebar today drop commits");return;}m_workspace.setView(QStringLiteral("today"));positionTask();}
                    else if(phase==318){if(!startDrag()){fail("upcoming drag setup");return;}}
                    else if(phase==319){dragPosition=targetPoint("navigation-scheduled");movePointer(QEvent::MouseMove,dragPosition);}
                    else if(phase==320){movePointer(QEvent::MouseButtonRelease,dragPosition);}
                    else if(phase==321){if(!picker->property("visible").toBool() || picker->property("field")!=QStringLiteral("upcoming")){fail("upcoming drop asks for future date");return;}QMetaObject::invokeMethod(picker,"dateSelected",Q_ARG(QString,m_workspace.dateOffset(2)),Q_ARG(QString,QStringLiteral("upcoming")));QMetaObject::invokeMethod(picker,"close");}
                    else if(phase==322){const auto task=m_workspace.task(dragSource);if(task.value("startOn").toString()!=m_workspace.dateOffset(2) || task.value("todayFocus").toBool()){fail("upcoming drop commits selected date");return;}QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(dragSource)));startDate->setProperty("text",QStringLiteral("2026-10-24"));deadline->setProperty("text",QStringLiteral("2026-11-20"));click("saveTaskEditor");}
                    else if(phase==323){if(m_workspace.task(dragSource).value("deadlineOn")!=QStringLiteral("2026-11-20")){fail("range task saved");return;}m_workspace.setView(QStringLiteral("month"));QMetaObject::invokeMethod(panel,"jumpToDate",Q_ARG(QVariant,QVariant(QStringLiteral("2026-10-24"))));}
                    else if(phase==324 || phase==326 || phase==328){
                        mainWindow->grabWindow();QMetaObject::invokeMethod(panel,"focusSelected");mainWindow->grabWindow();
                        std::function<QQuickItem*(QQuickItem*)> scan=[&](QQuickItem* item)->QQuickItem*{if(item->objectName()==QStringLiteral("calendarTaskRange-")+dragSource && item->isVisible())return item;for(auto* child:item->childItems())if(auto* found=scan(child))return found;return nullptr;};auto* band=scan(mainWindow->contentItem());
                        if(!band || band->width()<=0 || !capture(phase==324?QStringLiteral("task-range-month"):phase==326?QStringLiteral("task-range-dark-200"):QStringLiteral("task-range-without-events"))){fail("task range is rendered and remains visible without events");return;}
                        if(phase==324)QMetaObject::invokeMethod(band,"inspect");
                    } else if(phase==325){if(!editor->property("visible").toBool() || mainWindow->property("editId").toString()!=dragSource){fail("range opens the same complete task");return;}QMetaObject::invokeMethod(editor,"close");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);mainWindow->resize(880,580);}
                    else if(phase==327){panel->setProperty("showEvents",false);}
                    else if(phase==329){panel->setProperty("showEvents",true);m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);QMetaObject::invokeMethod(mainWindow,"moveTaskToNavigation",Q_ARG(QVariant,QVariant(dragSource)),Q_ARG(QVariant,m_workspace.task(dragSource).value("rowVersion")),Q_ARG(QVariant,QVariant(QStringLiteral("inbox"))));}
                    else if(phase==330){const auto task=m_workspace.task(dragSource);if(task.value("deadlineOn")!=QStringLiteral("2026-11-20") || task.value("startOn")!=QStringLiteral("2026-10-24") || !task.value("projectId").toString().isEmpty()){fail("inbox move preserves dates and clears classification");return;}m_workspace.setView(QStringLiteral("inbox"));}
                    else if(phase==331){if(m_workspace.taskIndex(dragSource)<0){fail("dated task belongs to default inbox");return;}m_workspace.setView(QStringLiteral("scheduled"));}
                    else if(phase==332){if(m_workspace.taskIndex(dragSource)<0){fail("dated inbox task also belongs to plan");return;}m_workspace.setView(QStringLiteral("inbox"));}
                } else if(phase>=333 && phase<=371) {
                    auto blankClick=[&]{auto* list=qobject_cast<QQuickItem*>(find("taskList"));mainWindow->grabWindow();const auto at=list->mapToScene(QPointF(list->width()/2,list->height()-8));QMouseEvent press(QEvent::MouseButtonPress,at,mainWindow->mapToGlobal(at),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);press.setTimestamp(inputClock->elapsed());QCoreApplication::sendEvent(mainWindow,&press);QMouseEvent release(QEvent::MouseButtonRelease,at,mainWindow->mapToGlobal(at),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);release.setTimestamp(inputClock->elapsed()+1);QCoreApplication::sendEvent(mainWindow,&release);};
                    auto category=[&]{for(const auto& value:m_workspace.categoryTree())if(value.toMap().value("title")==QStringLiteral("二级流程验收"))return value.toMap();return QVariantMap{};};
                    if(phase==333){mainWindow->resize(1120,800);m_workspace.setSearch(m_workspace.task(dragSource).value("title").toString());}
                    else if(phase==334){if(m_workspace.tasks()->rowCount()!=1)return;QMetaObject::invokeMethod(mainWindow,"openQuickEditor",Q_ARG(QVariant,QVariant(dragSource)));}
                    else if(phase==335){if(!quick->property("visible").toBool()){fail("blank click setup inline editor");return;}blankClick();}
                    else if(phase==336){if(quick->property("visible").toBool()){fail("blank click collapses unchanged inline task");return;}QMetaObject::invokeMethod(mainWindow,"openQuickEditor",Q_ARG(QVariant,QVariant(dragSource)));}
                    else if(phase==337){QMetaObject::invokeMethod(quick,"titleEdited",Q_ARG(QString,QStringLiteral("搜索离开自动保存")));blankClick();}
                    else if(phase==338){if(quick->property("visible").toBool() || editor->property("visible").toBool() || guard->property("visible").toBool() || m_workspace.task(dragSource).value("title")!=QStringLiteral("搜索离开自动保存")){fail("blank autosave exits even when edited title leaves search");return;}m_workspace.setSearch(QStringLiteral("搜索离开自动保存"));if(m_workspace.taskIndex(dragSource)<0)return;QMetaObject::invokeMethod(mainWindow,"openQuickEditor",Q_ARG(QVariant,QVariant(dragSource)));}
                    else if(phase==339){QMetaObject::invokeMethod(quick,"titleEdited",Q_ARG(QString,QString()));blankClick();}
                    else if(phase==340){if(!quick->property("visible").toBool() || m_workspace.errorText().isEmpty() || m_workspace.task(dragSource).value("title")!=QStringLiteral("搜索离开自动保存")){fail("invalid blank autosave stays in editor and preserves stored task");return;}QMetaObject::invokeMethod(quick,"titleEdited",Q_ARG(QString,m_workspace.task(dragSource).value("title").toString()));mainWindow->setProperty("draftDirty",false);QMetaObject::invokeMethod(quick,"close");m_workspace.clearError();m_workspace.setSearch(QString());QMetaObject::invokeMethod(mainWindow,"createChildCategory",Q_ARG(QVariant,QVariant(detailProjectId)));}
                    else if(phase==341){if(!creator->property("visible").toBool() || find("newCategoryParent")->property("currentIndex").toInt()<1 || !capture(QStringLiteral("category-create-child"))){fail("child creation carries parent context");return;}find("newCatalogName")->setProperty("text",QStringLiteral("二级流程验收"));click("createCatalogHere");}
                    else if(phase==342){const auto item=category();if(item.value("parentId").toString()!=detailProjectId || item.value("depth").toInt()!=1){fail("creator writes actual second-level category");return;}QMetaObject::invokeMethod(mainWindow,"manageCatalogItem",Q_ARG(QVariant,QVariant(item)),Q_ARG(QVariant,QVariant(true)));}
                    else if(phase==343){if(!find("categoryMoveDialog")->property("visible").toBool() || !capture(QStringLiteral("category-move-preview"))){fail("navigation opens category move preview");return;}find("categoryMoveParent")->setProperty("currentIndex",0);click("confirmCategoryMove");}
                    else if(phase==344){const auto item=category();auto* page=find("organizationPage");if(!item.value("parentId").toString().isEmpty() || page->property("selected").toMap().value("id")!=item.value("id") || find("categoryMoveDialog")->property("visible").toBool()){fail("promotion keeps selected category and closes preview");return;}QMetaObject::invokeMethod(page,"showMove");}
                    else if(phase==345){auto* page=find("organizationPage");const auto roots=page->property("moveRoots").toList();int target=-1;for(int i=0;i<roots.size();++i)if(roots[i].toMap().value("id")==detailProjectId)target=i+1;if(target<1){fail("demotion parent choices");return;}find("categoryMoveParent")->setProperty("currentIndex",target);click("confirmCategoryMove");}
                    else if(phase==346){if(category().value("parentId")!=detailProjectId){fail("demotion refreshes hierarchy");return;}QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("project:")+detailProjectId)));}
                    else if(phase==347){std::function<bool(QQuickItem*)> folders=[&](QQuickItem* item){if(item->objectName().startsWith(QStringLiteral("categoryNav-")) && item->property("symbol").isValid() && item->property("symbol").toString()!=QStringLiteral("folder"))return false;for(auto* child:item->childItems())if(!folders(child))return false;return true;};if(!folders(mainWindow->contentItem())){fail("all category levels share folder icon");return;}auto* toggle=qobject_cast<QQuickItem*>(find("toggleSidebar"));if(!toggle || toggle->mapToScene(QPointF(toggle->width(),0)).x()>mainWindow->property("sidebarWidth").toDouble()+1 || !capture(QStringLiteral("category-navigation-tree"))){fail("navigation owns its toggle");return;}auto* views=qobject_cast<QQuickItem*>(find("contentViewSwitch"));dragPosition=views->mapToScene(QPointF());click("showCalendarView");}
                    else if(phase==348){auto* views=qobject_cast<QQuickItem*>(find("contentViewSwitch"));if(QLineF(dragPosition,views->mapToScene(QPointF())).length()>1){fail("view icons remain fixed from list to calendar");return;}click("showListView");}
                    else if(phase==349){auto* views=qobject_cast<QQuickItem*>(find("contentViewSwitch"));if(QLineF(dragPosition,views->mapToScene(QPointF())).length()>1){fail("view icons remain fixed on return");return;}m_workspace.setSearch(QString());m_workspace.setView(QStringLiteral("inbox"));}
                    else if(phase==350){QMetaObject::invokeMethod(mainWindow,"manageCatalogItem",Q_ARG(QVariant,QVariant(category())),Q_ARG(QVariant,QVariant(false)));m_workspace.setThemeMode(2);m_workspace.setTextScale(2);mainWindow->resize(880,580);}
                    else if(phase==351){if(!capture(QStringLiteral("category-manager-dark-200"))){fail("large-text category manager");return;}QMetaObject::invokeMethod(mainWindow,"returnToContent");m_workspace.setThemeMode(1);m_workspace.setTextScale(1);mainWindow->resize(1120,720);}
                    else if(phase==352){
                        const auto nav=m_workspace.navigationCatalog();int source=-1;for(int i=0;i<nav.size();++i)if(nav[i].toMap().value("id")==category().value("id"))source=i;
                        int nearest=-1;for(int i=0;i<nav.size();++i){const auto item=nav[i].toMap();if(item.value("kind")!=QStringLiteral("project") || !item.value("parentId").toString().isEmpty() || item.value("id")==detailProjectId)continue;if(nearest<0 || qAbs(i-source)<qAbs(nearest-source))nearest=i;}
                        if(source<0 || nearest<0){fail("category drag source and adjacent root");return;}dragSource=category().value("id").toString();dragTarget=nav[nearest].toMap().value("id").toString();
                        std::function<QQuickItem*(QQuickItem*)> scan=[&](QQuickItem* item)->QQuickItem*{if(item->objectName()==QStringLiteral("categoryNav-")+dragSource)return item;for(auto* child:item->childItems())if(auto* found=scan(child))return found;return nullptr;};auto* row=scan(mainWindow->contentItem());auto* scroll=qvariant_cast<QObject*>(find("navigationScroll")->property("contentItem"));auto* flick=qobject_cast<QQuickItem*>(scroll);if(!row || !flick){fail("category drag geometry");return;}const auto y=row->mapToItem(qvariant_cast<QQuickItem*>(scroll->property("contentItem")),QPointF()).y();scroll->setProperty("contentY",qBound(0.0,y-flick->height()/2,qMax(0.0,scroll->property("contentHeight").toDouble()-flick->height())));
                        mainWindow->grabWindow();const auto at=row->mapToScene(QPointF(row->width()/2,row->height()/2));QMouseEvent rightPress(QEvent::MouseButtonPress,at,mainWindow->mapToGlobal(at),Qt::RightButton,Qt::RightButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&rightPress);QMouseEvent rightRelease(QEvent::MouseButtonRelease,at,mainWindow->mapToGlobal(at),Qt::RightButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&rightRelease);auto* context=find("categoryContextMenu");if(!context->property("visible").toBool() || context->property("x").toDouble()>mainWindow->property("sidebarWidth").toDouble()+1 || mainWindow->property("catalogContextItem").toMap().value("id")!=dragSource){qWarning()<<"Category row state"<<row->isEnabled()<<row->isVisible()<<row->property("showCatalogActions")<<mainWindow->property("toolModalOpen")<<mainWindow->isActive();QQuickItem* hit=mainWindow->contentItem();QPointF hitAt=at;while(auto* child=hit->childAt(hitAt.x(),hitAt.y())){hitAt=hit->mapToItem(child,hitAt);hit=child;qWarning()<<"Hit path"<<hit<<hit->size()<<hit->isVisible()<<hit->isEnabled();}qWarning()<<"Category context diagnostic"<<context->property("visible")<<context->property("x")<<mainWindow->property("sidebarWidth")<<mainWindow->property("catalogContextItem").toMap().value("id")<<dragSource<<at<<row->size()<<scroll->property("contentY")<<scroll->property("originY");capture(QStringLiteral("category-context-failure"));fail("actual category right click opens its own actions");return;}if(!capture(QStringLiteral("category-context-menu"))){fail("category context capture");return;}QMetaObject::invokeMethod(context,"close");
                    }
                    else if(phase>=353 && phase<=356){
                        auto rowFor=[&](const QString& id){std::function<QQuickItem*(QQuickItem*)> scan=[&](QQuickItem* item)->QQuickItem*{if(item->objectName()==QStringLiteral("categoryNav-")+id)return item;for(auto* child:item->childItems())if(auto* found=scan(child))return found;return nullptr;};return scan(mainWindow->contentItem());};mainWindow->grabWindow();auto* row=rowFor(dragSource);auto* target=rowFor(dragTarget);if(!row || !target){fail("category pointer targets");return;}
                        const auto start=row->mapToScene(QPointF(row->width()/2,row->height()/2)),end=target->mapToScene(QPointF(target->width()/2,target->height()/2));
                        if(phase==353){dragPosition=start;QMouseEvent press(QEvent::MouseButtonPress,start,mainWindow->mapToGlobal(start),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);press.setTimestamp(inputClock->elapsed());QCoreApplication::sendEvent(mainWindow,&press);}
                        else {const auto at=phase==354 ? start+(end-start)*0.5 : end;QMouseEvent move(QEvent::MouseMove,at,mainWindow->mapToGlobal(at),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);move.setTimestamp(inputClock->elapsed());QCoreApplication::sendEvent(mainWindow,&move);if(phase==355 && (!mainWindow->property("catalogDragActive").toBool() || !capture(QStringLiteral("category-drag-preview")))){fail("category pointer drag has preview");return;}if(phase==356){QMouseEvent release(QEvent::MouseButtonRelease,end,mainWindow->mapToGlobal(end),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);release.setTimestamp(inputClock->elapsed()+1);QCoreApplication::sendEvent(mainWindow,&release);}}
                    }
                    else if(phase==357){const auto item=category();if(item.value("parentId")!=dragTarget || mainWindow->property("catalogDragActive").toBool()){fail("pointer release reparents category atomically");return;}if(!capture(QStringLiteral("category-drag-result"))){fail("category drag result capture");return;}click("pageOptionsAction");mainWindow->grabWindow();click("toggleListSubtasks");}
                    else if(phase==358){if(m_workspace.preferences().value("listShowSubtasks").toBool()){fail("list child visibility menu action");return;}click("pageOptionsAction");mainWindow->grabWindow();click("toggleListSubtasks");m_workspace.setView(QStringLiteral("anytime"));}
                    else if(phase==359){
                        dragStepFirst.clear();for(int i=0;i<m_workspace.tasks()->rowCount();++i)if(m_workspace.tasks()->data(m_workspace.tasks()->index(i),TaskListModel::Depth).toInt()>0){dragStepFirst=m_workspace.tasks()->data(m_workspace.tasks()->index(i),TaskListModel::Id).toString();break;}
                        const auto index=m_workspace.taskIndex(dragStepFirst);if(index<0){fail("visible child fixture for inline indentation");return;}auto* list=find("taskList");list->setProperty("currentIndex",index);QMetaObject::invokeMethod(list,"positionViewAtIndex",Q_ARG(int,index),Q_ARG(int,4));
                    }
                    else if(phase==360){
                        mainWindow->grabWindow();auto* row=findRow(dragStepFirst);QVariant positions;
                        if(!row || !QMetaObject::invokeMethod(row,"columnPositions",Q_RETURN_ARG(QVariant,positions)) || positions.toMap().value("depth").toInt()<1 || !capture(QStringLiteral("task-list-hierarchy"))){fail("collapsed child column geometry");return;}inlineColumns=positions.toMap();QMetaObject::invokeMethod(mainWindow,"openQuickEditor",Q_ARG(QVariant,QVariant(dragStepFirst)));
                    }
                    else if(phase==361){
                        mainWindow->grabWindow();auto* row=findRow(dragStepFirst);QVariant positions;
                        if(!row || !row->property("expanded").toBool() || !QMetaObject::invokeMethod(row,"columnPositions",Q_RETURN_ARG(QVariant,positions))){fail("expanded child column geometry");return;}
                        const auto after=positions.toMap();if(qAbs(after.value("completion").toDouble()-inlineColumns.value("completion").toDouble())>1 || qAbs(after.value("title").toDouble()-inlineColumns.value("title").toDouble())>1 || !capture(QStringLiteral("child-inline-alignment"))){qCritical()<<"Child columns before/after"<<inlineColumns<<after;fail("inline child preserves completion and title columns");return;}
                        QMetaObject::invokeMethod(quick,"closeRequested");dragSource=dragTarget;dragTarget=detailProjectId;
                        // Source now has a child: its centre cannot be dropped
                        // into another category, but the edges must still sort.
                        auto* scroll=qvariant_cast<QObject*>(find("navigationScroll")->property("contentItem"));scroll->setProperty("contentY",0);
                        qint64 sourceVersion=0,targetVersion=0;for(const auto& value:m_workspace.categoryTree()){const auto item=value.toMap();if(item.value("id")==dragSource)sourceVersion=item.value("rowVersion").toLongLong();if(item.value("id")==dragTarget)targetVersion=item.value("rowVersion").toLongLong();}
                        // Normalize the temporary fixture so the first pointer
                        // gesture must actually move upward, regardless of UUID order.
                        if(!sourceVersion || !targetVersion || !m_workspace.dropCategory(dragSource,sourceVersion,dragTarget,targetVersion,QStringLiteral("after"))){fail("category upward movement fixture");return;}
                    }
                    else if(phase>=362 && phase<=369){
                        std::function<QQuickItem*(QQuickItem*,const QString&)> scan=[&](QQuickItem* item,const QString& id)->QQuickItem*{if(item->objectName()==QStringLiteral("categoryNav-")+id)return item;for(auto* child:item->childItems())if(auto* result=scan(child,id))return result;return nullptr;};
                        mainWindow->grabWindow();auto* row=scan(mainWindow->contentItem(),dragSource);auto* target=scan(mainWindow->contentItem(),dragTarget);if(!row || !target){fail("category reorder pointer targets");return;}
                        const bool before=phase<=365;const int step=(phase-362)%4;
                        if(step==0){auto* viewport=qobject_cast<QQuickItem*>(qvariant_cast<QObject*>(find("navigationScroll")->property("contentItem")));if(!viewport){fail("category navigation viewport");return;}const auto minY=qMin(row->mapToItem(viewport,QPointF()).y(),target->mapToItem(viewport,QPointF()).y()),maxY=qMax(row->mapToItem(viewport,QPointF(0,row->height())).y(),target->mapToItem(viewport,QPointF(0,target->height())).y());const auto current=viewport->property("contentY").toDouble();if(minY<40)viewport->setProperty("contentY",qMax(0.0,current+minY-40));else if(maxY>viewport->height()-40)viewport->setProperty("contentY",current+maxY-viewport->height()+40);mainWindow->grabWindow();}
                        const auto start=row->mapToScene(QPointF(row->width()*0.6,row->height()/2)),centre=target->mapToScene(QPointF(target->width()*0.6,target->height()/2));
                        auto send=[&](QEvent::Type type,const QPointF& at){QMouseEvent event(type,at,mainWindow->mapToGlobal(at),type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);event.setTimestamp(inputClock->elapsed());QCoreApplication::sendEvent(mainWindow,&event);};
                        if(step==0){
                            if(!row->property("hasChildren").toBool()){fail("reorder fixture retains child category");return;}
                            if(before && start.y()<=centre.y()){fail("upward drag starts below its target");return;}
                            if(!before){const auto nav=m_workspace.navigationCatalog();int source=-1,destination=-1;for(int i=0;i<nav.size();++i){const auto item=nav[i].toMap();if(item.value("id")==dragSource){source=i;if(!item.value("parentId").toString().isEmpty()){fail("upward edge preserves root level");return;}}if(item.value("id")==dragTarget)destination=i;}if(source<0 || destination<0 || source>=destination){fail("actual upward release persists category order");return;}}
                            send(QEvent::MouseButtonPress,start);send(QEvent::MouseMove,start+(centre-start)*0.5);
                        }
                        else if(step==1)send(QEvent::MouseMove,centre);
                        else if(step==2){if(!mainWindow->property("catalogDragActive").toBool() || !target->property("catalogDropActive").toBool() || target->property("catalogDropAllowed").toBool()){qCritical()<<"Category drag centre"<<phase<<start<<centre<<row->property("catalogDragging")<<target->property("catalogDropActive")<<target->property("catalogDropAllowed");fail("invalid category centre remains tracked without allowing nesting");return;}dragPosition=target->mapToScene(QPointF(target->width()*0.6,before?2:target->height()-2));send(QEvent::MouseMove,dragPosition);}
                        else {if(!target->property("catalogDropAllowed").toBool() || target->property("catalogDropPlacement").toString()!=(before?QStringLiteral("before"):QStringLiteral("after")) || !capture(before?QStringLiteral("category-insert-before"):QStringLiteral("category-insert-after"))){fail("same-row edge recovers valid category insertion");return;}send(QEvent::MouseButtonRelease,dragPosition);}
                    }
                    else if(phase==370){
                        const auto nav=m_workspace.navigationCatalog();int source=-1,target=-1;for(int i=0;i<nav.size();++i){const auto item=nav[i].toMap();if(item.value("id")==dragSource){source=i;if(!item.value("parentId").toString().isEmpty()){fail("edge sorting preserves root level");return;}}if(item.value("id")==dragTarget)target=i;}
                        if(source<0 || target<0 || source<=target || mainWindow->property("catalogDragActive").toBool()){fail("actual downward release persists category order");return;}m_workspace.reload();
                    }
                    else if(phase==371){
                        const auto nav=m_workspace.navigationCatalog();int source=-1,target=-1;for(int i=0;i<nav.size();++i){if(nav[i].toMap().value("id")==dragSource)source=i;if(nav[i].toMap().value("id")==dragTarget)target=i;}if(source<=target || target<0 || !capture(QStringLiteral("category-reordered-compact-navigation"))){fail("pointer category order survives reload");return;}
                    }

                } else if(phase>=372 && phase<=435) {
                    auto position=[&]{QMetaObject::invokeMethod(mainWindow,"revealTaskForVerification",Q_ARG(QVariant,QVariant(dragSource)));};
                    auto verifyForm=[&](QObject* surface){int rows=0;if(!surface || !surface->property("visible").toBool())return false;for(auto* row:surface->findChildren<QQuickItem*>(QStringLiteral("desktopFormRow"))){if(!row->isVisible())continue;QVariant result;if(!QMetaObject::invokeMethod(row,"verifyLayout",Q_RETURN_ARG(QVariant,result)) || result.toString()!=QStringLiteral("PASS")){qCritical()<<"Form row geometry:"<<result;return false;}++rows;}return rows>0;};
                    if(phase==372){m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("today"))));m_workspace.createTask(QStringLiteral("父子联动验收"));}
                    else if(phase==373){dragSource.clear();for(const auto& task:m_workspace.tasks()->items())if(task.title==QStringLiteral("父子联动验收"))dragSource=task.id;if(dragSource.isEmpty()){fail("tree UI fixture");return;}QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(dragSource)));QMetaObject::invokeMethod(mainWindow,"selectEditorTab",Q_ARG(QVariant,QVariant(1)));find("newSubtaskTitle")->setProperty("text",QStringLiteral("无独立日期的子任务"));QMetaObject::invokeMethod(find("newSubtaskTitle"),"accepted");}
                    else if(phase==374){const auto children=m_workspace.subtaskChoices(dragSource);if(children.size()!=1 || mainWindow->property("editorTab").toInt()!=1){fail("real child creation tab");return;}dragStepFirst=children.first().toMap().value("taskId").toString();QMetaObject::invokeMethod(editor,"close");QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("today"))));position();}
                    else if(phase==375){mainWindow->grabWindow();auto* row=findRow(dragSource);auto* link=row?row->findChild<QQuickItem*>(QStringLiteral("taskSubtasksLink-")+dragSource):nullptr;if(m_workspace.taskIndex(dragStepFirst)>=0 || !link || !link->isVisible() || find("toggleListSubtasks")->property("visible").toBool()){qCritical()<<"Smart child link"<<m_workspace.taskIndex(dragSource)<<m_workspace.taskIndex(dragStepFirst)<<row<<link<<(link ? link->isVisible() : false)<<m_workspace.tasks()->rowCount();fail("smart view keeps exact child membership and visible number link");return;}const auto at=link->mapToScene(QPointF(link->width()/2,link->height()/2));QMouseEvent press(QEvent::MouseButtonPress,at,mainWindow->mapToGlobal(at),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&press);QMouseEvent release(QEvent::MouseButtonRelease,at,mainWindow->mapToGlobal(at),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&release);}
                    else if(phase==376){if(!editor->property("visible").toBool() || mainWindow->property("editorTab").toInt()!=1 || mainWindow->property("editorRootId").toString()!=dragSource || !capture(QStringLiteral("smart-subtask-link-panel"))){fail("subtask link opens same parent child panel");return;}QMetaObject::invokeMethod(editor,"close");QMetaObject::invokeMethod(mainWindow,"requestDeleteTask",Q_ARG(QVariant,QVariant(dragSource)));if(!find("deleteParentConfirm")->property("visible").toBool()){fail("parent deletion explains whole subtree");return;}click("cancelDeleteParent");}
                    else if(phase==377){if(m_workspace.task(dragSource).isEmpty() || m_workspace.task(dragStepFirst).isEmpty()){fail("cancel parent delete changes nothing");return;}QMetaObject::invokeMethod(mainWindow,"requestDeleteTask",Q_ARG(QVariant,QVariant(dragSource)));click("confirmDeleteParent");}
                    else if(phase==378){if(!m_workspace.task(dragSource).isEmpty() || !m_workspace.task(dragStepFirst).isEmpty()){fail("confirmed parent delete removes entire tree from live cache");return;}m_workspace.setView(QStringLiteral("trash"));position();}
                    else if(phase==379){mainWindow->grabWindow();auto* row=findRow(dragSource);auto* restore=row?row->findChild<QObject*>(QStringLiteral("taskDetails-")+dragSource):nullptr;if(!restore || !row->property("deleted").toBool() || m_workspace.taskIndex(dragStepFirst)<0 || !capture(QStringLiteral("recently-deleted-tree"))){fail("recently deleted exposes actual tree recovery");return;}QMetaObject::invokeMethod(restore,"clicked");}
                    else if(phase==380){if(m_workspace.task(dragStepFirst).value("parentTaskId").toString()!=dragSource || m_workspace.task(dragSource).isEmpty()){fail("recently deleted recovery restores whole tree and cache");return;}QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("all"))));m_workspace.setTaskChildrenExpanded(dragSource,true);m_workspace.setPreference(QStringLiteral("listShowSubtasks"),true);position();}
                    else if(phase==381){const auto child=m_workspace.taskIndex(dragStepFirst);if(child<0 || m_workspace.tasks()->items()[child].listDepth!=1 || !capture(QStringLiteral("all-grouped-category-tree"))){fail("ordinary all view displays child indentation");return;}auto* row=findRow(dragSource);if(!row){fail("parent completion row");return;}QMetaObject::invokeMethod(row,"completeRequested");}
                    else if(phase==382){if(m_workspace.task(dragSource).value("taskStatus")!=QStringLiteral("COMPLETED") || m_workspace.task(dragStepFirst).value("taskStatus")!=QStringLiteral("COMPLETED")){fail("row completion cascades and refreshes all descendants");return;}QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("completed"))));position();}
                    else if(phase==383){if(m_workspace.taskIndex(dragStepFirst)<0 || !capture(QStringLiteral("completed-by-month"))){fail("completed view includes finished child");return;}auto* row=findRow(dragSource);if(!row){fail("completed parent row");return;}QMetaObject::invokeMethod(row,"completeRequested");}
                    else if(phase==384){if(m_workspace.task(dragStepFirst).value("taskStatus")!=QStringLiteral("OPEN")){fail("reopening completed parent restores same-operation child");return;}QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("scheduled"))));position();}
                    else if(phase==385){if(m_workspace.taskIndex(dragSource)<0 || m_workspace.taskIndex(dragStepFirst)>=0 || !capture(QStringLiteral("scheduled-date-groups"))){fail("plan has its own precise dated membership");return;}const auto item=m_workspace.task(dragSource);m_workspace.saveDetails(dragSource,item.value("rowVersion").toLongLong(),{{"important",true}});}
                    else if(phase==386){QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("important"))));position();}
                    else if(phase==387){if(m_workspace.taskIndex(dragSource)<0 || m_workspace.taskIndex(dragStepFirst)>=0 || !capture(QStringLiteral("flagged-view"))){fail("flagged view does not inherit importance to children");return;}std::function<QQuickItem*(QQuickItem*,const QString&)> tile=[&](QQuickItem* item,const QString& name)->QQuickItem*{if(item->objectName()==name)return item;for(auto* child:item->childItems())if(auto* found=tile(child,name))return found;return nullptr;};for(const char* name:{"navigation-today","navigation-scheduled","navigation-all","navigation-important","navigation-completed"})if(!tile(mainWindow->contentItem(),QString::fromLatin1(name))){fail("five smart view navigation tiles");return;}QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("today"))));position();}
                    else if(phase==388){mainWindow->grabWindow();auto* row=findRow(dragSource);QVariant geometry;if(!row || !QMetaObject::invokeMethod(row,"verifyGeometry",Q_RETURN_ARG(QVariant,geometry)) || geometry.toString()!=QStringLiteral("PASS") || !capture(QStringLiteral("today-compact-smart-view"))){qCritical()<<"Task row geometry"<<geometry;fail("compact smart task row metadata geometry");return;}m_workspace.setTextScale(2);m_workspace.setThemeMode(2);mainWindow->resize(880,580);QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("all"))));position();}
                    else if(phase==389){mainWindow->grabWindow();auto* row=findRow(dragSource);if(!row){position();return;}QVariant geometry;if(!row || !QMetaObject::invokeMethod(row,"verifyGeometry",Q_RETURN_ARG(QVariant,geometry)) || geometry.toString()!=QStringLiteral("PASS") || !capture(QStringLiteral("compact-all-dark-200"))){qCritical()<<"Large row geometry:"<<geometry<<row<<m_workspace.taskIndex(dragSource)<<find("taskList")->property("contentY")<<find("taskList")->property("originY");capture(QStringLiteral("compact-tree-geometry-failure"));fail("compact tree large-text row geometry");return;}}
                    else if(phase==390){m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);QVariantMap category;for(const auto& value:m_workspace.categoryTree())if(value.toMap().value("id").toString()==detailProjectId)category=value.toMap();if(category.isEmpty()){fail("local category edit fixture");return;}QMetaObject::invokeMethod(mainWindow,"manageCatalogItem",Q_ARG(QVariant,QVariant(category)),Q_ARG(QVariant,QVariant(false)));}
                    else if(phase==391){if(!find("catalogEditPopup")->property("visible").toBool()){fail("local category editor visible");return;}find("catalogEditName")->setProperty("text",QString());QMetaObject::invokeMethod(find("catalogEditName"),"textEdited");click("saveCatalogEdit");}
                    else if(phase==392){if(!find("catalogEditPopup")->property("visible").toBool() || m_workspace.errorText().isEmpty()){fail("category validation failure retains local draft");return;}find("catalogEditName")->setProperty("text",QStringLiteral("就近分类编辑已保存"));QMetaObject::invokeMethod(find("catalogEditName"),"textEdited");click("saveCatalogEdit");}
                    else if(phase==393){activateSmokeWindow(mainWindow);QVariantMap category;for(const auto& value:m_workspace.categoryTree())if(value.toMap().value("id").toString()==detailProjectId)category=value.toMap();if(find("catalogEditPopup")->property("visible").toBool() || category.value("title").toString()!=QStringLiteral("就近分类编辑已保存")){fail("local category editor persists and closes on success");return;}QMetaObject::invokeMethod(mainWindow,"manageCatalogItem",Q_ARG(QVariant,QVariant(category)),Q_ARG(QVariant,QVariant(false)));}
                    else if(phase==394){static bool diagnosed=false;if(!diagnosed){diagnosed=true;qInfo()<<"Category keyboard precondition:"<<mainWindow->isActive()<<find("catalogEditPopup")->property("visible")<<find("catalogEditPopup")->property("opened")<<QGuiApplication::focusWindow();}if(QGuiApplication::platformName()!=QStringLiteral("offscreen") && !mainWindow->isActive()){activateSmokeWindow(mainWindow);return;}if(!find("catalogEditPopup")->property("opened").toBool())return;find("catalogEditName")->setProperty("text",QStringLiteral("未保存的就近修改"));QMetaObject::invokeMethod(find("catalogEditName"),"textEdited");QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QCoreApplication::sendEvent(mainWindow,&escape);}
                    else if(phase==395){if(!find("catalogEditDiscard")->property("visible").toBool() || !find("catalogEditPopup")->property("visible").toBool()){qCritical()<<"Category escape:"<<find("catalogEditPopup")->property("dirty")<<find("catalogEditPopup")->property("visible")<<find("catalogEditPopup")->property("opened")<<find("catalogEditDiscard")->property("visible")<<mainWindow->isActive()<<QGuiApplication::focusWindow();capture(QStringLiteral("category-escape-failure"));fail("escape protects local category draft");return;}QMetaObject::invokeMethod(find("catalogEditDiscard"),"reject");if(find("catalogEditName")->property("text").toString()!=QStringLiteral("未保存的就近修改")){fail("continue local edit preserves input");return;}QMetaObject::invokeMethod(find("catalogEditPopup"),"requestClose");QMetaObject::invokeMethod(find("catalogEditDiscard"),"accept");}
                    else if(phase==396){for(const auto& value:m_workspace.categoryTree())if(value.toMap().value("id").toString()==detailProjectId && value.toMap().value("title").toString()!=QStringLiteral("就近分类编辑已保存")){fail("discard local category edit is zero modification");return;}if(find("catalogEditPopup")->property("visible").toBool()){fail("discard closes local editor");return;}QMetaObject::invokeMethod(find("showArchivedCategories"),"triggered");}
                    else if(phase==397){if(mainWindow->property("toolPage").toString()!=QStringLiteral("organization") || !find("organizationPage")->property("archivedOnly").toBool() || !capture(QStringLiteral("archived-category-recovery"))){fail("section context retains archived category recovery entry");return;}}
                    else if(phase==398){m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("all"))));QMetaObject::invokeMethod(mainWindow,"openTool",Q_ARG(QVariant,QVariant(QStringLiteral("settings"))));QMetaObject::invokeMethod(find("settingsDialog"),"setVerificationCategory",Q_ARG(QVariant,QVariant(0)));}
                    else if(phase==399){if(!verifyForm(find("settingsDialog")) || !capture(QStringLiteral("desktop-settings-light"))){fail("desktop settings aligned form");return;}QMetaObject::invokeMethod(find("settingsDialog"),"close");QMetaObject::invokeMethod(find("catalogCreateDialog"),"showForLibrary",Q_ARG(QVariant,QVariant(QStringLiteral("project"))));}
                    else if(phase==400){if(!verifyForm(find("catalogCreateDialog")) || !capture(QStringLiteral("desktop-new-category"))){fail("desktop category creation form");return;}QMetaObject::invokeMethod(find("catalogCreateDialog"),"close");QMetaObject::invokeMethod(find("catalogCreateDialog"),"showForLibrary",Q_ARG(QVariant,QVariant(QStringLiteral("tag"))));}
                    else if(phase==401){find("newCatalogName")->setProperty("text",QStringLiteral("配色创建验收"));auto* colors=find("newCatalogColors");QVariant activated;if(!colors || !QMetaObject::invokeMethod(colors,"activateForVerification",Q_RETURN_ARG(QVariant,activated),Q_ARG(QVariant,QVariant(3))) || !activated.toBool() || colors->property("currentIndex").toInt()!=3 || !verifyForm(find("catalogCreateDialog")) || !capture(QStringLiteral("desktop-new-tag"))){fail("new tag swatch chooses actual color");return;}click("createCatalogHere");}
                    else if(phase==402){bool saved=false;for(const auto& value:m_workspace.catalogOf(QStringLiteral("tag"))){const auto tag=value.toMap();if(tag.value("title").toString()==QStringLiteral("配色创建验收") && tag.value("colorKey").toString()==QStringLiteral("orange"))saved=true;}if(!saved || find("catalogCreateDialog")->property("visible").toBool()){fail("new tag color persists");return;}m_workspace.setTextScale(2);m_workspace.setThemeMode(2);mainWindow->resize(880,580);QMetaObject::invokeMethod(find("catalogCreateDialog"),"showForLibrary",Q_ARG(QVariant,QVariant(QStringLiteral("tag"))));}
                    else if(phase==403){if(!verifyForm(find("catalogCreateDialog")) || !capture(QStringLiteral("desktop-new-tag-dark-200"))){fail("large text creation form remains usable");return;}QMetaObject::invokeMethod(find("catalogCreateDialog"),"close");QMetaObject::invokeMethod(mainWindow,"openTool",Q_ARG(QVariant,QVariant(QStringLiteral("data"))));}
                    else if(phase==404){auto* tools=find("dataToolsDialog");auto* footer=tools ? qvariant_cast<QQuickItem*>(tools->property("footer")) : nullptr;if(!tools || !tools->property("visible").toBool() || !footer || !footer->isVisible() || footer->mapToScene(QPointF(0,footer->height())).y()>mainWindow->height()+1 || !capture(QStringLiteral("desktop-data-dark-200"))){fail("data sheet fixed actions remain in window");return;}QMetaObject::invokeMethod(tools,"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("month"))));find("calendarPanel")->setProperty("agendaVisible",false);}
                    else if(phase==405){mainWindow->grabWindow();auto* panel=find("calendarPanel");auto* agenda=panel ? panel->findChild<QQuickItem*>(QStringLiteral("calendarAgenda")) : nullptr;QVariant grid;if(!panel || !agenda || agenda->isVisible() || !QMetaObject::invokeMethod(panel,"verifyGridStart",Q_RETURN_ARG(QVariant,grid)) || grid.toString()!=QStringLiteral("PASS") || !capture(QStringLiteral("desktop-month"))){fail("calendar defaults to content grid");return;}QMetaObject::invokeMethod(panel,"showAgenda");}
                    else if(phase==406){auto* panel=find("calendarPanel");auto* agenda=panel ? panel->findChild<QQuickItem*>(QStringLiteral("calendarAgenda")) : nullptr;if(!agenda || !agenda->isVisible() || agenda->width()<=0 || !capture(QStringLiteral("desktop-agenda"))){qCritical()<<"Agenda discovery:"<<panel<<(panel ? panel->property("agendaVisible") : QVariant())<<agenda<<(agenda ? agenda->isVisible() : false)<<(agenda ? agenda->size() : QSizeF());fail("calendar agenda remains discoverable");return;}}
                    else if(phase==407){m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));QMetaObject::invokeMethod(mainWindow,"selectEditorTab",Q_ARG(QVariant,QVariant(1)));}
                    else if(phase==408){mainWindow->grabWindow();const auto children=m_workspace.subtaskChoices(secondId);if(children.isEmpty() || find("taskEditorScroll")->property("visible").toBool() || !capture(QStringLiteral("compact-child-list"))){fail("single child list surface");return;}dragStepFirst=children.first().toMap().value("taskId").toString();QVariant opened;if(!QMetaObject::invokeMethod(find("subtaskList"),"openForVerification",Q_RETURN_ARG(QVariant,opened),Q_ARG(QVariant,QVariant(dragStepFirst))) || !opened.toBool()){fail("compact child row activation");return;}}
                    else if(phase==409){mainWindow->grabWindow();QVariant layout;if(!QMetaObject::invokeMethod(editor,"verifyLeafDetails",Q_RETURN_ARG(QVariant,layout)) || layout.toString()!=QStringLiteral("PASS") || find("taskEditorTabs")->property("visible").toBool() || !capture(QStringLiteral("compact-child-details"))){qCritical()<<"Leaf details:"<<layout;fail("child detail replaces list and remains visible");return;}m_workspace.setTextScale(2);m_workspace.setThemeMode(2);mainWindow->resize(880,580);}
                    else if(phase==410){mainWindow->grabWindow();QVariant layout;if(!QMetaObject::invokeMethod(editor,"verifyLeafDetails",Q_RETURN_ARG(QVariant,layout)) || layout.toString()!=QStringLiteral("PASS") || !capture(QStringLiteral("compact-child-details-dark-200"))){qCritical()<<"Large leaf details:"<<layout;fail("large child detail visible without nested child controls");return;}click("returnToParentSubtasks");}
                    else if(phase==411){if(mainWindow->property("editId").toString()!=secondId || mainWindow->property("editorTab").toInt()!=1 || !find("newSubtaskTitle")->property("visible").toBool() || !capture(QStringLiteral("compact-child-list-dark-200"))){fail("large child return restores parent list and creation");return;}QMetaObject::invokeMethod(editor,"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("all"))));}
                    else if(phase==412){if(mainWindow->property("sidebarWidth").toDouble()!=200 || !capture(QStringLiteral("compact-sources-navigation"))){fail("compact source navigation");return;}}
                    else if(phase>=413){
                        std::function<QQuickItem*(QQuickItem*,const QString&)> visual=[&](QQuickItem* item,const QString& name)->QQuickItem*{if(item->objectName()==name && item->isVisible())return item;for(auto* child:item->childItems())if(auto* found=visual(child,name))return found;return nullptr;};
                        auto mouse=[&](QEvent::Type type,const QPointF& at,Qt::MouseButton button,Qt::MouseButtons buttons){QMouseEvent event(type,at,mainWindow->mapToGlobal(at),button,buttons,Qt::NoModifier);event.setTimestamp(inputClock->elapsed());QCoreApplication::sendEvent(mainWindow,&event);};
                        auto right=[&](QQuickItem* item){if(!item || !item->isVisible())return false;if(item->objectName().startsWith(QStringLiteral("categoryNav-")) || item->objectName().startsWith(QStringLiteral("tagNav-"))){auto* scroll=qvariant_cast<QObject*>(find("navigationScroll")->property("contentItem"));auto* flick=qobject_cast<QQuickItem*>(scroll);if(!flick)return false;const auto y=item->mapToItem(qvariant_cast<QQuickItem*>(scroll->property("contentItem")),QPointF()).y();scroll->setProperty("contentY",qBound(0.0,y-flick->height()/2,qMax(0.0,scroll->property("contentHeight").toDouble()-flick->height())));mainWindow->grabWindow();}const auto at=item->mapToScene(QPointF(item->width()*0.45,item->height()/2));mouse(QEvent::MouseButtonPress,at,Qt::RightButton,Qt::RightButton);mouse(QEvent::MouseButtonRelease,at,Qt::RightButton,Qt::NoButton);return true;};
                        auto taskAction=[&](const QString& id,const char* prefix){auto* row=findRow(id);auto* menu=row?row->findChild<QObject*>(QStringLiteral("taskContext-")+id):nullptr;auto* action=row?row->findChild<QObject*>(QString::fromLatin1(prefix)+id):nullptr;if(!menu || !menu->property("visible").toBool() || !action || !action->property("enabled").toBool())return false;const auto invoked=QMetaObject::invokeMethod(action,"triggered");QMetaObject::invokeMethod(menu,"close");return invoked;};
                        auto reopen=[&]{auto* row=findRow(scheduleId);auto* list=qobject_cast<QQuickItem*>(find("taskList"));if(!row || !list || row->mapToScene(QPointF()).y()<list->mapToScene(QPointF()).y() || row->mapToScene(QPointF(0,row->height())).y()>list->mapToScene(QPointF(0,list->height())).y()){QMetaObject::invokeMethod(mainWindow,"revealTaskForVerification",Q_ARG(QVariant,QVariant(scheduleId)));return false;}return right(row);};
                        auto reveal=[&](const QString& id){QMetaObject::invokeMethod(mainWindow,"revealTaskForVerification",Q_ARG(QVariant,QVariant(id)));};
                        if(phase==413){probe->setInterval(60);QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("inbox"))));m_workspace.createTask(QStringLiteral("统一时间与操作验收"));}
                        else if(phase==414){scheduleId.clear();for(const auto& task:m_workspace.tasks()->items())if(task.title==QStringLiteral("统一时间与操作验收"))scheduleId=task.id;if(scheduleId.isEmpty()){fail("shared time fixture identity");return;}QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(scheduleId)));startDate->setProperty("text",m_workspace.dateOffset(2));find("editDeadlineDate")->setProperty("text",m_workspace.dateOffset(2));find("taskAllDay")->setProperty("checked",true);QMetaObject::invokeMethod(find("taskAllDay"),"clicked");find("taskTimeStart")->setProperty("text",QStringLiteral("09:00"));find("taskTimeEnd")->setProperty("text",QStringLiteral("10:00"));reminder->setProperty("text",m_workspace.dateOffset(2)+QStringLiteral(" 08:30"));click("saveTaskEditor");}
                        else if(phase==415){const auto task=m_workspace.task(scheduleId);const auto timing=task.value("schedule").toMap();if(editor->property("visible").toBool() || task.value("startOn")!=timing.value("startDate") || task.value("deadlineOn")!=timing.value("endDate") || timing.value("allDay").toBool() || task.value("remindAtUtc").toString().isEmpty()){fail("one visible date pair saves the same calendar interval");return;}reveal(scheduleId);}
                        else if(phase==416){mainWindow->grabWindow();if(!reopen()){static QElapsedTimer wait;if(!wait.isValid())wait.start();if(wait.elapsed()<1000)return;fail("actual task right pointer");return;}}
                        else if(phase==417){auto* row=findRow(scheduleId);auto* menu=row?row->findChild<QObject*>(QStringLiteral("taskContext-")+scheduleId):nullptr;if(!menu || !menu->property("visible").toBool() || quick->property("editing").toBool() || !capture(QStringLiteral("task-direct-context"))){fail("right click opens nearby menu without editing");return;}QMetaObject::invokeMethod(menu,"close");row->forceActiveFocus();QKeyEvent key(QEvent::KeyPress,Qt::Key_F10,Qt::ShiftModifier);QCoreApplication::sendEvent(mainWindow,&key);QKeyEvent keyRelease(QEvent::KeyRelease,Qt::Key_F10,Qt::ShiftModifier);QCoreApplication::sendEvent(mainWindow,&keyRelease);if(!menu->property("visible").toBool() || !taskAction(scheduleId,"taskMenuPin-")){fail("Shift-F10 opens the same task menu and pins");return;}}
                        else if(phase==418){if(!m_workspace.task(scheduleId).value("pinned").toBool()){fail("context pin persists");return;}if(!reopen())return;if(!taskAction(scheduleId,"taskMenuImportant-")){fail("context important command");return;}}
                        else if(phase==419){if(!m_workspace.task(scheduleId).value("important").toBool()){fail("context importance persists");return;}if(!reopen())return;if(!taskAction(scheduleId,"taskMenuCancelReminder-")){fail("context cancel reminder command");return;}}
                        else if(phase==420){const auto task=m_workspace.task(scheduleId);if(!task.value("remindAtUtc").toString().isEmpty() || task.value("reminderRepeat").toString()!=QStringLiteral("NONE") || task.value("schedule").toMap().value("alertMinutes").toInt()!=-1 ){fail("cancel reminder clears repeating and relative reminder");return;}if(!reopen())return;if(!taskAction(scheduleId,"taskMenuPin-")){fail("context unpin command");return;}}
                        else if(phase==421){if(m_workspace.task(scheduleId).value("pinned").toBool()){fail("context unpin persists");return;}dragSource.clear();dragTarget.clear();const auto tasks=m_workspace.tasks()->items();for(int i=0;i+1<tasks.size();++i)if(tasks[i].parentTaskId.isEmpty() && tasks[i+1].parentTaskId.isEmpty() && !tasks[i].pinned && !tasks[i+1].pinned){dragSource=tasks[i].id;dragTarget=tasks[i+1].id;break;}if(dragSource.isEmpty()){fail("ordinary direct drag pair");return;}reveal(dragSource);reveal(dragTarget);}
                        else if(phase==422){mainWindow->grabWindow();auto* source=findRow(dragSource);auto* target=findRow(dragTarget);if(!source || !target || !source->property("canReorder").toBool()){fail("ordinary rows accept drag");return;}const auto from=source->mapToScene(QPointF(source->width()*0.45,source->height()/2)),to=target->mapToScene(QPointF(target->width()*0.45,target->height()/2));mouse(QEvent::MouseButtonPress,from,Qt::LeftButton,Qt::LeftButton);mouse(QEvent::MouseMove,from+QPointF(0,m_workspace.dragDistance()+4),Qt::NoButton,Qt::LeftButton);mouse(QEvent::MouseMove,to,Qt::NoButton,Qt::LeftButton);if(!source->property("dragging").toBool() || quick->property("editing").toBool() || !capture(QStringLiteral("ordinary-immediate-drag"))){fail("moving beyond system threshold starts drag without hold");return;}mouse(QEvent::MouseButtonRelease,to,Qt::LeftButton,Qt::NoButton);}
                        else if(phase==423){if(m_workspace.taskIndex(dragSource)!=m_workspace.taskIndex(dragTarget)+1){fail("actual ordinary pointer drop changes order");return;}m_workspace.reload();}
                        else if(phase==424){if(m_workspace.taskIndex(dragSource)!=m_workspace.taskIndex(dragTarget)+1){fail("ordinary pointer order reloads");return;}auto* row=visual(mainWindow->contentItem(),QStringLiteral("categoryNav-")+detailProjectId);if(!right(row) || mainWindow->property("catalogContextItem").toMap().value("id").toString()!=detailProjectId){fail("category rename starts from actual right click");return;}quickOriginalTask=mainWindow->property("catalogContextItem").toMap();QMetaObject::invokeMethod(find("renameCatalog"),"triggered");QMetaObject::invokeMethod(find("categoryContextMenu"),"close");}
                        else if(phase==425){auto* name=find("catalogEditName");if(!find("catalogEditPopup")->property("visible").toBool() || !name->property("activeFocus").toBool() || name->property("selectedText")!=quickOriginalTask.value("title")){fail("rename selects existing category name");return;}name->setProperty("text",QStringLiteral("右键重命名分类"));QMetaObject::invokeMethod(name,"textEdited");click("saveCatalogEdit");}
                        else if(phase==426){QVariantMap current;for(const auto& value:m_workspace.categoryTree())if(value.toMap().value("id").toString()==detailProjectId)current=value.toMap();if(current.value("title").toString()!=QStringLiteral("右键重命名分类") || current.value("parentId")!=quickOriginalTask.value("parentId") || current.value("description")!=quickOriginalTask.value("description")){fail("category rename preserves hierarchy and metadata");return;}const auto tags=m_workspace.catalogOf("tag");if(tags.isEmpty()){fail("tag rename fixture");return;}dragStepFirst=tags.first().toMap().value("id").toString();auto* row=visual(mainWindow->contentItem(),QStringLiteral("tagNav-")+dragStepFirst);if(!right(row)){fail("tag actual right click");return;}quickOriginalTask=mainWindow->property("catalogContextItem").toMap();QMetaObject::invokeMethod(find("renameCatalog"),"triggered");QMetaObject::invokeMethod(find("categoryContextMenu"),"close");}
                        else if(phase==427){auto* name=find("catalogEditName");if(mainWindow->property("catalogContextItem").toMap().value("id").toString()!=dragStepFirst || !find("catalogEditPopup")->property("visible").toBool() || !name->property("activeFocus").toBool() || name->property("selectedText")!=quickOriginalTask.value("title") || !capture(QStringLiteral("tag-rename-nearby"))){qCritical()<<"Tag rename"<<mainWindow->property("catalogContextItem")<<dragStepFirst<<find("catalogEditPopup")->property("visible")<<name->property("activeFocus")<<name->property("selectedText")<<quickOriginalTask.value("title");fail("tag rename focused and selected");return;}name->setProperty("text",QStringLiteral("右键重命名标签"));QMetaObject::invokeMethod(name,"textEdited");click("saveCatalogEdit");}
                        else if(phase==428){QVariantMap current;for(const auto& value:m_workspace.catalogOf("tag"))if(value.toMap().value("id").toString()==dragStepFirst)current=value.toMap();if(current.value("title").toString()!=QStringLiteral("右键重命名标签") || current.value("colorKey")!=quickOriginalTask.value("colorKey")){fail("tag rename preserves color and identity");return;}QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("month"))));auto* panel=find("calendarPanel");panel->setProperty("showTasks",true);panel->setProperty("showEvents",true);panel->setProperty("agendaVisible",false);QMetaObject::invokeMethod(panel,"jumpToDate",Q_ARG(QVariant,QVariant(m_workspace.dateOffset(2))));}
                        else if(phase==429){mainWindow->grabWindow();auto* marker=visual(mainWindow->contentItem(),QStringLiteral("calendarTaskMark-")+scheduleId);if(!marker){fail("same timed task is visible in calendar");return;}const auto at=marker->mapToScene(QPointF(marker->width()/2,marker->height()/2));mouse(QEvent::MouseButtonPress,at,Qt::LeftButton,Qt::LeftButton);mouse(QEvent::MouseButtonRelease,at,Qt::LeftButton,Qt::NoButton);mouse(QEvent::MouseButtonPress,at,Qt::LeftButton,Qt::LeftButton);mouse(QEvent::MouseButtonDblClick,at,Qt::LeftButton,Qt::LeftButton);mouse(QEvent::MouseButtonRelease,at,Qt::LeftButton,Qt::NoButton);}
                        else if(phase==430){mainWindow->grabWindow();if(!editor->property("visible").toBool() || mainWindow->property("editId").toString()!=scheduleId || startDate->property("text").toString()!=m_workspace.dateOffset(2) || !find("taskAllDay")->property("checked").toBool() || !capture(QStringLiteral("shared-task-time-light"))){fail("calendar click opens same task with single time controls");return;}m_workspace.setTextScale(2);m_workspace.setThemeMode(2);mainWindow->resize(880,580);}
                        else if(phase==431){mainWindow->grabWindow();QMetaObject::invokeMethod(mainWindow,"revealTimingForVerification");}
                        else if(phase==432){mainWindow->grabWindow();if(!capture(QStringLiteral("shared-task-time-dark-200"))){fail("shared time large dark capture");return;}QMetaObject::invokeMethod(editor,"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);mainWindow->resize(1120,720);QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("inbox"))));reveal(scheduleId);}
                        else if(phase==433){mainWindow->grabWindow();if(!reopen())return;if(!taskAction(scheduleId,"taskMenuDelete-")){fail("actual task context delete command");return;}}
                        else if(phase==434){if(!m_workspace.task(scheduleId).isEmpty()){fail("context delete soft deletes same task");return;}QMetaObject::invokeMethod(mainWindow,"changeView",Q_ARG(QVariant,QVariant(QStringLiteral("trash"))));reveal(scheduleId);}
                        else if(phase==435){if(m_workspace.taskIndex(scheduleId)<0 || !capture(QStringLiteral("context-delete-recoverable"))){fail("context deletion is recoverable in recently deleted");return;}}
                    }





                }
            } else if(phase>=191) {
                auto* fields=mainWindow->findChild<QObject*>(QStringLiteral("taskDetailsFields"));
                auto* reminder=mainWindow->findChild<QObject*>(QStringLiteral("editorReminder"));
                auto* save=mainWindow->findChild<QObject*>(QStringLiteral("saveTaskEditor"));
                auto catalogId=[&](const QString& kind){for(const auto& value:m_workspace.catalogOf(kind))if(value.toMap().value(QStringLiteral("title")).toString().startsWith(QStringLiteral("布局验收")))return value.toMap().value(QStringLiteral("id")).toString();return QString();};
                auto capture=[&](const QString& name){return screenshotPath.isEmpty() || mainWindow->grabWindow().save(screenshotPath+QLatin1Char('.')+name+QStringLiteral(".png"));};
                auto open=[&]{
                    if(!QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId))))return false;
                    // Exercise endpoint layout explicitly; text scaling no longer
                    // changes the user's inspector-size or date-section choice.
                    if(!mainWindow->property("inspectorExpanded").toBool() && !mainWindow->property("dateTimeExpanded").toBool())
                        return QMetaObject::invokeMethod(mainWindow->findChild<QObject*>(QStringLiteral("editorTimeSummary")),"clicked");
                    return true;
                };
                auto select=[&](const QString& kind,const QString& id,const QString& heading){QVariant result;return QMetaObject::invokeMethod(fields,"selectDestinationForVerification",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,QVariant(kind)),Q_ARG(QVariant,QVariant(id)),Q_ARG(QVariant,QVariant(heading))) && result.toBool();};
                auto layout=[&](bool wide){QVariant result;const bool called=QMetaObject::invokeMethod(mainWindow,"verifyTaskDetailLayout",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,QVariant(wide)));if(!called || result.toString()!=QStringLiteral("PASS"))qWarning()<<"Detail layout:"<<result;return called && result.toString()==QStringLiteral("PASS");};
                if(phase==191) {if(!m_workspace.saveCatalog({{"kind",QStringLiteral("project")},{"title",QStringLiteral("布局验收分类")},{"planningState",QStringLiteral("ACTIVE")},{"status",QStringLiteral("OPEN")}})){fail("detail project setup");return;}}
                else if(phase==192) {detailProjectId=catalogId(QStringLiteral("project"));if(detailProjectId.isEmpty() || !m_workspace.saveCatalog({{"kind",QStringLiteral("heading")},{"title",QStringLiteral("布局验收分组")},{"parentId",detailProjectId}})){fail("detail heading setup");return;}}
                else if(phase==193) {detailHeadingId=catalogId(QStringLiteral("heading"));if(detailHeadingId.isEmpty() || !m_workspace.saveCatalog({{"kind",QStringLiteral("area")},{"title",QStringLiteral("布局验收一级")}})){fail("detail area setup");return;}}
                else if(phase==194) {detailAreaId=catalogId(QStringLiteral("area"));m_workspace.setView(QStringLiteral("month"));mainWindow->resize(1120,900);if(detailAreaId.isEmpty() || !open()){fail("detail full page setup");return;}}
                else if(phase==195) {
                    mainWindow->grabWindow();if(!layout(false) || fields->property("infoExpanded").toBool() || !capture(QStringLiteral("detail-layout-page")) || !select(QStringLiteral("project"),detailProjectId,detailHeadingId)){fail("detail aligned page and project selection");return;}
                    startDate->setProperty("text",m_workspace.dateOffset(0));reminder->setProperty("text",m_workspace.localReminder(QDateTime::currentDateTimeUtc().addSecs(3600).toString(Qt::ISODateWithMs)));QMetaObject::invokeMethod(save,"clicked");
                } else if(phase==196) {
                    const auto task=m_workspace.task(secondId);if(editor->property("visible").toBool() || task.value(QStringLiteral("projectId")).toString()!=detailProjectId || task.value(QStringLiteral("headingId")).toString()!=detailHeadingId || !task.value(QStringLiteral("areaId")).toString().isEmpty() || task.value(QStringLiteral("startOn")).toString()!=m_workspace.dateOffset(0) || task.value(QStringLiteral("remindAtUtc")).toString().isEmpty() || !open()){qCritical()<<"Detail saved fields"<<editor->property("visible")<<task.value(QStringLiteral("projectId"))<<task.value(QStringLiteral("areaId"))<<task.value(QStringLiteral("headingId"))<<task.value(QStringLiteral("evening"))<<task.value(QStringLiteral("remindAtUtc"))<<m_workspace.errorText();fail("detail atomic project and moved scheduling fields");return;}
                } else if(phase==197) {
                    if(!fields->findChild<QObject*>(QStringLiteral("editorHeadingRow"))->property("visible").toBool() || fields->findChild<QObject*>(QStringLiteral("editorHeading"))->property("currentIndex").toInt()!=1 || !capture(QStringLiteral("detail-layout-project"))){fail("detail existing assignment restored");return;}
                    auto* info=mainWindow->findChild<QObject*>(QStringLiteral("taskInfoToggle"));QMetaObject::invokeMethod(info,"triggered");m_workspace.setThemeMode(2);m_workspace.setTextScale(2);mainWindow->resize(880,580);
                } else if(phase==198) {
                    static int detailFrames=0;mainWindow->grabWindow();if(++detailFrames<3)return;
                    if(!layout(false) || !fields->property("infoExpanded").toBool() || !capture(QStringLiteral("detail-layout-dark-200"))){fail("detail responsive draft and history popup");return;}QMetaObject::invokeMethod(mainWindow->findChild<QObject*>(QStringLiteral("taskEditHistoryDialog")),"close");if(!select(QStringLiteral("area"),detailAreaId,QString())){fail("legacy category selection");return;}
                    reminder->setProperty("text",QStringLiteral("invalid"));QMetaObject::invokeMethod(save,"clicked");
                } else if(phase==199) {
                    if(!editor->property("visible").toBool() || m_workspace.errorText().isEmpty() || m_workspace.task(secondId).value(QStringLiteral("projectId")).toString()!=detailProjectId || fields->findChild<QObject*>(QStringLiteral("editorHeadingRow"))->property("visible").toBool()){fail("detail failed area draft preserves stored project");return;}
                    reminder->setProperty("text",QString());QMetaObject::invokeMethod(save,"clicked");
                } else if(phase==200) {
                    const auto task=m_workspace.task(secondId);if(editor->property("visible").toBool() || task.value(QStringLiteral("areaId")).toString()!=detailAreaId || !task.value(QStringLiteral("projectId")).toString().isEmpty() || !task.value(QStringLiteral("headingId")).toString().isEmpty() || !task.value(QStringLiteral("remindAtUtc")).toString().isEmpty() || task.value(QStringLiteral("startOn")).toString()!=m_workspace.dateOffset(0)){fail("detail atomic mutually exclusive area commit");return;}
                    m_workspace.setThemeMode(1);m_workspace.setTextScale(1);m_workspace.setView(QStringLiteral("today"));mainWindow->resize(1120,720);open();
                } else if(phase==201) {
                    static int dockFrames=0;mainWindow->grabWindow();if(++dockFrames<3)return;
                    if(!editor->property("visible").toBool() || !layout(false) || !capture(QStringLiteral("detail-layout-docked"))){fail("detail narrow docked layout");return;}
                    startDate->setProperty("text",m_workspace.dateOffset(1));
                    if(startDate->property("text").toString()!=m_workspace.dateOffset(1) || mainWindow->property("draftTodayFocus").toBool() || mainWindow->property("draftEvening").toBool()){fail("removing focus uses date-only C++ decision and clears tonight draft");return;}
                    mainWindow->setProperty("draftDirty",false);QMetaObject::invokeMethod(editor,"close");
                }
            } else if(phase>=180) {
                auto capture=[&](const QString& suffix){return screenshotPath.isEmpty() || mainWindow->grabWindow().save(screenshotPath+QLatin1Char('.')+suffix+QStringLiteral(".png"));};
                auto* gallery=mainWindow->findChild<QObject*>(QStringLiteral("styleGallery"));
                auto* settings=mainWindow->findChild<QObject*>(QStringLiteral("settingsDialog"));
                auto* contrast=mainWindow->findChild<QObject*>(QStringLiteral("highContrastPreference"));
                if(!gallery || !settings || !contrast){fail("action and contrast objects");return;}
                if(phase==180) {
                    m_workspace.setView(QStringLiteral("today"));m_workspace.setThemeMode(1);mainWindow->resize(1120,720);
                    mainWindow->findChild<QObject*>(QStringLiteral("newTaskTitle"))->setProperty("text",QStringLiteral("图标添加验证"));
                } else if(phase==181) {
                    auto* add=mainWindow->findChild<QQuickItem*>(QStringLiteral("quickAddTask"));
                    if(!add || !add->isEnabled() || add->width()<32 || add->height()<32 || add->property("symbol").toString()!=QStringLiteral("plus") || !QMetaObject::invokeMethod(add,"clicked")){fail("icon quick add command");return;}
                } else if(phase==182) {
                    bool found=false;for(int row=0;row<m_workspace.tasks()->rowCount();++row)if(m_workspace.tasks()->data(m_workspace.tasks()->index(row),TaskListModel::Title).toString()==QStringLiteral("图标添加验证"))found=true;
                    if(!found || !mainWindow->findChild<QObject*>(QStringLiteral("newTaskTitle"))->property("text").toString().isEmpty()){fail("icon quick add committed and cleared");return;}
                    QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(QStringLiteral("style"))),Q_ARG(QVariant,QVariant(taskId)));
                } else if(phase==183) {
                    QVariant result;if(!QMetaObject::invokeMethod(gallery,"verifyActions",Q_RETURN_ARG(QVariant,result)) || result.toString()!=QStringLiteral("PASS") || !capture(QStringLiteral("actions-light"))){fail("icon names and action layout");return;}
                    m_workspace.setThemeMode(2);
                } else if(phase==184) {
                    if(!capture(QStringLiteral("actions-dark"))){fail("dark action capture");return;}
                    m_workspace.setPreference(QStringLiteral("highContrast"),true);
                } else if(phase==185) {
                    if(!capture(QStringLiteral("actions-dark-contrast"))){fail("dark contrast capture");return;}
                    m_workspace.setThemeMode(1);m_workspace.setTextScale(2);mainWindow->resize(880,580);
                } else if(phase==186) {
                    if(!capture(QStringLiteral("actions-contrast-200"))){fail("large contrast capture");return;}
                    m_workspace.setTextScale(1);mainWindow->resize(1120,720);QMetaObject::invokeMethod(gallery,"close");
                    QMetaObject::invokeMethod(mainWindow,"openTool",Q_ARG(QVariant,QVariant(QStringLiteral("settings"))));
                    QMetaObject::invokeMethod(settings,"setVerificationCategory",Q_ARG(QVariant,QVariant(0)));
                } else if(phase==187) {
                    auto* calendar=mainWindow->findChild<QQuickWindow*>(QStringLiteral("desktopCalendarWindow"));auto* background=calendar->findChild<QObject*>(QStringLiteral("desktopCalendarBackground"));
                    if(!contrast->property("checked").toBool() || !background || background->property("color").value<QColor>().alphaF()<0.999 || m_workspace.preferences().value(QStringLiteral("desktopCalendarOpacity")).toInt()!=70 || !capture(QStringLiteral("settings-contrast"))){fail("contrast solid surface preserves opacity preference");return;}
                    QMetaObject::invokeMethod(contrast,"toggle");QMetaObject::invokeMethod(contrast,"clicked");
                } else if(phase==188) {
                    auto* background=mainWindow->findChild<QQuickWindow*>(QStringLiteral("desktopCalendarWindow"))->findChild<QObject*>(QStringLiteral("desktopCalendarBackground"));
                    if(m_workspace.preferences().value(QStringLiteral("highContrast")).toBool() || contrast->property("checked").toBool() || qAbs(background->property("color").value<QColor>().alphaF()-0.7)>0.01){fail("contrast toggle restores transparency");return;}
                    QMetaObject::invokeMethod(contrast,"toggle");QMetaObject::invokeMethod(contrast,"clicked");
                } else if(phase==189) {
                    if(!m_workspace.preferences().value(QStringLiteral("highContrast")).toBool()){fail("contrast checkbox enables preference");return;}
                    QMetaObject::invokeMethod(contrast,"toggle");QMetaObject::invokeMethod(contrast,"clicked");QMetaObject::invokeMethod(settings,"close");
                } else if(phase==190) {
                    if(m_workspace.preferences().value(QStringLiteral("highContrast")).toBool()){fail("contrast preference restored");return;}
                }
            } else if(phase>=132) {
                auto* extra=mainWindow->findChild<QObject*>(QStringLiteral("taskDetailsFields"));
                auto* newStep=mainWindow->findChild<QQuickItem*>(phase>=139?QStringLiteral("editorImportant"):QStringLiteral("editTaskNotes"));
                auto* reminder=mainWindow->findChild<QObject*>(QStringLiteral("editorReminder"));
                auto* save=mainWindow->findChild<QObject*>(QStringLiteral("saveTaskEditor"));
                auto* scroll=mainWindow->findChild<QQuickItem*>(QStringLiteral("taskEditorScroll"));
                auto* notes=mainWindow->findChild<QObject*>(QStringLiteral("editTaskNotes"));
                if(!extra || !newStep || !reminder || !save || !scroll || !notes){fail("unified editor objects");return;}
                auto capture=[&](const QString& suffix){return screenshotPath.isEmpty() || mainWindow->grabWindow().save(screenshotPath+QLatin1Char('.')+suffix+QStringLiteral(".png"));};
                auto open=[&]{return QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(secondId)));};
                auto click=[&](const char* name){auto* button=mainWindow->findChild<QObject*>(QString::fromLatin1(name));return button && QMetaObject::invokeMethod(button,"clicked");};
                auto navigate=[&]{QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");};
                auto* flick=static_cast<QObject*>(scroll);
                const auto rowName=QStringLiteral("taskRow-")+secondId;
                std::function<QQuickItem*(QQuickItem*)> findRow=[&](QQuickItem* item)->QQuickItem*{if(!item)return nullptr;if(item->objectName()==rowName)return item;for(auto* child:item->childItems())if(auto* found=findRow(child))return found;return nullptr;};
                if(phase==132){m_workspace.setView(QStringLiteral("inbox"));m_workspace.createTask(QStringLiteral("统一编辑原始"));}
                else if(phase==133){
                    secondId.clear();for(const auto& task:m_workspace.tasks()->items())if(task.title==QStringLiteral("统一编辑原始"))secondId=task.id;
                    if(secondId.isEmpty() || !open()){fail("unified editor open");return;}
                    editTitle->setProperty("text",QStringLiteral("统一编辑完成"));newStep->setProperty("text",QStringLiteral("步骤 A"));
                    reminder->setProperty("text",QStringLiteral("invalid"));QMetaObject::invokeMethod(save,"clicked");
                } else if(phase==134){
                    const auto task=m_workspace.task(secondId);
                    if(!editor->property("visible").toBool() || m_workspace.errorText().isEmpty() || task.value(QStringLiteral("title")).toString()!=QStringLiteral("统一编辑原始") || !task.value(QStringLiteral("notes")).toString().isEmpty() || editTitle->property("text").toString()!=QStringLiteral("统一编辑完成")){fail("unified failed save preserves all drafts");return;}
                    if(!capture(QStringLiteral("unified-error"))){fail("fixed editor error");return;}
                    reminder->setProperty("text",QString());QMetaObject::invokeMethod(save,"clicked");
                } else if(phase==135){
                    const auto task=m_workspace.task(secondId);
                    if(editor->property("visible").toBool() || task.value(QStringLiteral("title")).toString()!=QStringLiteral("统一编辑完成") || task.value(QStringLiteral("notes")).toString()!=QStringLiteral("步骤 A") || task.value(QStringLiteral("rowVersion")).toLongLong()!=2){fail("unified atomic UI save");return;}
                    open();newStep->setProperty("text",QStringLiteral("取消后的草稿"));navigate();
                } else if(phase==136){if(!click("cancelLeave")){fail("unified cancel navigation");return;}}
                else if(phase==137){if(!editor->property("visible").toBool() || newStep->property("text").toString()!=QStringLiteral("取消后的草稿")){fail("unified notes draft retained");return;}navigate();}
                else if(phase==138){if(!click("discardDraft")){fail("unified discard navigation");return;}}
                else if(phase==139){
                    if(editor->property("visible").toBool() || m_workspace.task(secondId).value(QStringLiteral("notes")).toString()!=QStringLiteral("步骤 A")){fail("unified discard zero mutation");return;}
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
                    const bool overflow=flick && flick->property("contentHeight").toDouble()>flick->property("height").toDouble()+1;
                    auto* editorBar=mainWindow->findChild<QQuickItem*>(QStringLiteral("editorScrollBar"));
                    if(scrollingBodies!=1 || !flick || flick->property("interactive").toBool()!=overflow || (!overflow && (!editorBar || editorBar->isVisible() || qAbs(flick->property("contentY").toDouble())>1)) || fieldTop<viewportTop-1 || fieldTop+newStep->height()>viewportTop+scroll->height()+1){qCritical()<<"phase"<<phase<<"bodies"<<scrollingBodies<<"top"<<fieldTop<<viewportTop<<"scroll"<<flick->property("contentY")<<flick->property("contentHeight")<<flick->property("height")<<"focus"<<mainWindow->activeFocusItem()<<newStep->hasActiveFocus();fail("single editor scroll and visible keyboard focus");return;}
                    if(phase==140){if(!capture(QStringLiteral("unified-fields"))){fail("unified fields capture");return;}mainWindow->resize(880,580);}
                    else if(phase==141){if(!capture(QStringLiteral("unified-page"))){fail("unified page capture");return;}m_workspace.setTextScale(2);}
                    else if(phase==142){if(!capture(QStringLiteral("unified-page-200"))){fail("large unified page capture");return;}m_workspace.setTextScale(1);mainWindow->resize(1440,900);}
                    else {if(!capture(QStringLiteral("unified-wide"))){fail("wide unified capture");return;}mainWindow->setProperty("draftDirty",false);QMetaObject::invokeMethod(editor,"close");mainWindow->resize(1120,720);auto* list=mainWindow->findChild<QObject*>(QStringLiteral("taskList"));if(list){QMetaObject::invokeMethod(list,"forceLayout");}QMetaObject::invokeMethod(mainWindow,"revealTaskForVerification",Q_ARG(QVariant,QVariant(secondId)));}
                } else if(phase==144){auto* row=findRow(mainWindow->contentItem());if(!row || !QMetaObject::invokeMethod(row,"openVerificationMenu")){fail("task row menu open");return;}}
                else if(phase==145){
                    auto* row=findRow(mainWindow->contentItem());QVariant result;
                    if(!row || !QMetaObject::invokeMethod(row,"verifyMenuSpacing",Q_RETURN_ARG(QVariant,result)) || result.toString()!=QStringLiteral("PASS")){qCritical()<<result;fail("hidden task menu items leave no gaps");return;}
                    if(!screenshotPath.isEmpty())for(auto* window:QGuiApplication::allWindows())if(auto* popup=qobject_cast<QQuickWindow*>(window);popup && popup!=mainWindow && popup->isVisible() && popup->objectName()!=QStringLiteral("widgetWindow"))if(!popup->grabWindow().save(screenshotPath+QStringLiteral(".task-menu.png"))){fail("task menu capture");return;}
                    QMetaObject::invokeMethod(row,"closeVerificationMenu");
                } else if(phase==146)m_workspace.setView(QStringLiteral("today"));
                else if(phase==147) {
                    m_workspace.setView(QStringLiteral("inbox"));m_workspace.setSearch(m_workspace.task(secondId).value("title").toString());open();notes->setProperty("text",QStringLiteral("短备注"));
                    // Exercise wheel propagation with an actually overflowing
                    // body at a constrained height; natural-height panels may fit
                    // these short fields in a taller window.
                    mainWindow->resize(1120,580);
                    mainWindow->setProperty("dateTimeExpanded",true);
                    QMetaObject::invokeMethod(editTitle,"forceActiveFocus");
                    QMetaObject::invokeMethod(mainWindow,"ensureEditorFocus",Q_ARG(QVariant,QVariant::fromValue(editTitle)));
                } else if(phase==148 || phase==149) {
                    // Native wheel motion is animated: a timer callback alone
                    // doesn't establish that the updated scene was presented.
                    if(phase==149 && QGuiApplication::platformName()!=QStringLiteral("offscreen") && wheelFrames->load()<2) {
                        mainWindow->requestUpdate();
                        return;
                    }
                    mainWindow->grabWindow();
                    auto* target=phase==148 ? qobject_cast<QQuickItem*>(notes) : mainWindow->findChild<QQuickItem*>(QStringLiteral("editStartDate"));
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
                        QObject::disconnect(*wheelConnection);
                        // Wheel motion is animated by the Flickable's own physics, so the
                        // offset advances over subsequent frames rather than inside the
                        // callback that delivered the event. Wait for the scene to settle
                        // instead of sampling once - the single-sample version failed
                        // intermittently on software-rendered (offscreen) runs.
                        if(flick->property("contentY").toDouble()<=1) {
                            static QElapsedTimer wheelSettle;
                            if(!wheelSettle.isValid())wheelSettle.start();
                            mainWindow->requestUpdate();
                            if(wheelSettle.elapsed()<2000)return;
                            qCritical()<<"Wheel geometry"<<flick->property("contentY")<<flick->property("contentHeight")<<flick->property("height")<<qobject_cast<QQuickItem*>(notes)->mapToScene(QPointF(0,0))<<mainWindow->activeFocusItem()<<mainWindow->property("editorManualScroll");fail("notes wheel must scroll outer body");return;
                        }
                        QMetaObject::invokeMethod(target,"forceActiveFocus");
                        if(target->property("wheelEnabled").toBool() || target->property("currentIndex").toInt()!=0){fail("combo wheel policy");return;}
                    }
                    const auto position=target->mapToScene(QPointF(target->width()/2,target->height()/2));
                    QWheelEvent wheel(position,mainWindow->mapToGlobal(position.toPoint()),QPoint(0,-96),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                    // Flickable uses event timestamps to calculate wheel velocity.
                    // Zero timestamps can make successive synthetic events share a time.
                    wheel.setTimestamp(static_cast<quint64>(inputClock->elapsed()));
                    if(phase==148 && QGuiApplication::platformName()!=QStringLiteral("offscreen")) {
                        wheelFrames->store(0);
                        *wheelConnection=QObject::connect(mainWindow,&QQuickWindow::frameSwapped,probe,[wheelFrames]{wheelFrames->fetch_add(1);},Qt::DirectConnection);
                    }
                    QCoreApplication::sendEvent(mainWindow,&wheel);
                    qInfo()<<"Synthetic wheel"<<phase<<wheel.isAccepted()<<mainWindow->isActive()<<flick->property("contentY")<<mainWindow->property("editorManualScroll");
                } else if(phase==150) {
                    auto* date=mainWindow->findChild<QObject*>(QStringLiteral("editStartDate"));
                    if(!date || date->property("text").toString()!=m_workspace.task(secondId).value("startOn").toString() || flick->property("contentY").toDouble()<=1){fail("wheel preserves choice and manual scroll");return;}
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
                    mainWindow->setProperty("draftDirty",false);QMetaObject::invokeMethod(editor,"close");m_workspace.setSearch(QString());m_workspace.setView(QStringLiteral("today"));
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
                    auto* center=mainWindow->findChild<QObject*>(QStringLiteral("reminderPage"));if(!center || !center->property("visible").toBool() || !capture(QStringLiteral("hig-reminders"))){fail("reminder history content page");return;}
                    QMetaObject::invokeMethod(center,"close");QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(taskId)));
                    auto* repeating=mainWindow->findChild<QObject*>(QStringLiteral("taskRepeating"));QMetaObject::invokeMethod(repeating,"toggle");QMetaObject::invokeMethod(repeating,"clicked");
                    QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");
                } else if(phase>=156 && phase<=158) {
                    auto* guard=mainWindow->findChild<QObject*>(QStringLiteral("unsavedChangesDialog"));
                    if(phase==156){if(!guard->property("visible").toBool() || !mainWindow->property("draftDirty").toBool()){fail("repeat task draft guards page navigation");return;}QMetaObject::invokeMethod(mainWindow->findChild<QObject*>(QStringLiteral("cancelLeave")),"clicked");}
                    else if(phase==157){if(!editor->property("visible").toBool() || !mainWindow->findChild<QObject*>(QStringLiteral("taskRepeating"))->property("checked").toBool()){fail("cancel retains task repeat draft");return;}QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");QMetaObject::invokeMethod(mainWindow->findChild<QObject*>(QStringLiteral("discardDraft")),"clicked");QMetaObject::invokeMethod(mainWindow,"openTool",Q_ARG(QVariant,QVariant(QStringLiteral("notifications"))));}
                    else {if(editor->property("visible").toBool() || mainWindow->property("toolPage").toString()!=QStringLiteral("notifications") || m_workspace.task(taskId).value("repeat").toMap().value("active").toBool()){fail("discarded repeat never writes a template");return;}QMetaObject::invokeMethod(mainWindow,"returnToContent");m_workspace.setView(QStringLiteral("today"));}
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
                    QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(eventId)));
                    auto* repeat=mainWindow->findChild<QObject*>(QStringLiteral("taskRepeating"));QMetaObject::invokeMethod(repeat,"toggle");QMetaObject::invokeMethod(repeat,"clicked");auto* frequency=mainWindow->findChild<QObject*>(QStringLiteral("taskRepeatFrequency"));frequency->setProperty("currentIndex",1);QMetaObject::invokeMethod(frequency,"activated",Q_ARG(int,1));QMetaObject::invokeMethod(mainWindow,"saveEditor");
                } else if(phase==166) {
                    if(m_workspace.task(eventId).value("repeat").toMap().value("frequency").toString()!=QStringLiteral("WEEKLY")){fail("task repeat shares calendar entity");return;}QMetaObject::invokeMethod(mainWindow,"openEditor",Q_ARG(QVariant,QVariant(eventId)));
                } else if(phase==167 || phase==169) {
                    static int renderedPhase=-1,frames=0;mainWindow->grabWindow();if(renderedPhase!=phase){renderedPhase=phase;frames=0;return;}if(++frames<3)return;QVariant visible;if(!QMetaObject::invokeMethod(mainWindow,"revealTimingForVerification",Q_RETURN_ARG(QVariant,visible)) || !visible.toBool()){fail("shared timing fields laid out at both scales");return;}
                } else if(phase==168) {
                    if(!capture(QStringLiteral("event-series"))){fail("series capture");return;}
                    m_workspace.setTextScale(2);m_workspace.setThemeMode(2);

                } else if(phase==170) {
                    if(!capture(QStringLiteral("task-time-repeat-dark-200"))){fail("large task timing capture");return;}QMetaObject::invokeMethod(editor,"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_workspace.setView(QStringLiteral("month"));
                } else if(phase==171) {
                    if(!capture(QStringLiteral("calendar-event-bands"))){fail("calendar bands capture");return;}
                    mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->setProperty("showTasks",true);
                } else if(phase==172) {
                    const auto original=m_workspace.task(eventId);const auto rule=original.value("recurrenceId").toString();const auto date=QDate::fromString(original.value("startOn").toString(),Qt::ISODate).addDays(7).toString(Qt::ISODate);auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));QVariant jumped;QMetaObject::invokeMethod(panel,"jumpToDate",Q_RETURN_ARG(QVariant,jumped),Q_ARG(QVariant,QVariant(date)));
                    QString id;for(const auto& day:m_workspace.calendarDaysFor(date,false))if(day.toMap().value("date").toString()==date)for(const auto& item:day.toMap().value("events").toList()){const auto row=item.toMap();if(m_workspace.task(row.value("taskId").toString()).value("recurrenceId").toString()==rule)id=row.value("taskId").toString();}
                    if(!jumped.toBool() || id.isEmpty() || id==eventId){fail("later calendar occurrence owns a distinct task ID");return;}QMetaObject::invokeMethod(panel,"inspectEvent",Q_ARG(QVariant,QVariant(id)),Q_ARG(QVariant,QVariant(QString())));
                } else if(phase==173) {
                    const auto id=mainWindow->property("editId").toString();if(!editor->property("visible").toBool() || id==eventId || m_workspace.task(id).value("recurrenceId")!=m_workspace.task(eventId).value("recurrenceId") || !capture(QStringLiteral("calendar-instance-task"))){fail("calendar opens actual occurrence in shared details");return;}m_workspace.setTextScale(2);m_workspace.setThemeMode(2);
                } else if(phase==174) {
                    static int frames=0;mainWindow->grabWindow();if(++frames<3)return;auto* save=mainWindow->findChild<QQuickItem*>(QStringLiteral("saveTaskEditor"));if(!save || save->mapToScene(QPointF(0,save->height())).y()>mainWindow->height()+1 || !capture(QStringLiteral("calendar-instance-task-dark-200"))){fail("shared task save reachable at 200 percent");return;}
                } else if(phase==175) {
                    if(!editor->property("visible").toBool()){fail("same detail panel remains open");return;}QMetaObject::invokeMethod(editor,"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));QMetaObject::invokeMethod(panel->findChild<QObject*>(QStringLiteral("calendarJump")),"clicked");panel->findChild<QObject*>(QStringLiteral("calendarJumpDate"))->setProperty("text",QStringLiteral("2026-02-30"));QMetaObject::invokeMethod(panel->findChild<QObject*>(QStringLiteral("confirmCalendarJump")),"clicked");
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
                    auto* calendar=mainWindow->findChild<QQuickWindow*>(QStringLiteral("desktopCalendarWindow"));calendar->grabWindow();auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));auto* independent=mainWindow->findChild<QObject*>(QStringLiteral("desktopCalendarPanel"));if(calendar->width()!=420 || calendar->height()!=480 || !editor->property("visible").toBool() || mainWindow->property("editId").toString()!=eventId || !capture(QStringLiteral("desktop-opens-shared-details-200"))){fail("desktop calendar opens the same task editor");return;}QMetaObject::invokeMethod(editor,"close");m_workspace.setTextScale(1);if(independent->property("anchorDate").toString()!=QStringLiteral("2024-02-29") || panel->property("selectedDate").toString()!=QStringLiteral("2028-02-29")){fail("independent date jump preserves main calendar");return;}QMetaObject::invokeMethod(calendar,"close");m_workspace.selectCalendarDate(m_workspace.calendarEvent(eventId).value("startDate").toString());
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
                    const bool nativeFocus=QGuiApplication::platformName()==QStringLiteral("offscreen") || mainWindow->activeFocusItem()!=nullptr;
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
                auto* eventEditor=mainWindow->findChild<QObject*>(QStringLiteral("taskEditor"));
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
                    auto* title=editTitle;auto* save=mainWindow->findChild<QObject*>(QStringLiteral("saveTaskEditor"));
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
                else if(phase==106){auto* panel=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"));QVariant titles;if(panel){QMetaObject::invokeMethod(panel,"verifyDayTitles",Q_RETURN_ARG(QVariant,titles));if(titles.toString()==QStringLiteral("WAIT"))return;}if(!panel || !QMetaObject::invokeMethod(panel,"verifyDayTitles",Q_RETURN_ARG(QVariant,titles)) || titles.toString()!=QStringLiteral("PASS")){qWarning()<<"Calendar title check:"<<titles;fail("compact calendar concrete titles");return;}if(!capture(QStringLiteral("calendar-200")) || !panel || !QMetaObject::invokeMethod(panel,"showAgenda")){fail("large calendar agenda entry");return;}}
                else if(phase==107){auto* agenda=mainWindow->findChild<QObject*>(QStringLiteral("calendarPanel"))->findChild<QQuickItem*>(QStringLiteral("calendarAgenda"));if(!agenda || agenda->mapToScene(QPointF(0,0)).y()<0 || agenda->mapToScene(QPointF(0,0)).y()>mainWindow->height() || !capture(QStringLiteral("calendar-agenda-200")) || !show(QStringLiteral("event"),eventId)){qCritical()<<"Agenda geometry:"<<(agenda ? agenda->mapToScene(QPointF(0,0)) : QPointF())<<(agenda ? agenda->size() : QSizeF());fail("large agenda reachable");return;}}
                else if(phase==108){if(!capture(QStringLiteral("calendar-event-200"))){fail("large event form");return;}editTitle->setProperty("text",QString());QMetaObject::invokeMethod(mainWindow,"close");}
                else if(phase==109){QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");auto* discard=mainWindow->findChild<QObject*>(QStringLiteral("unsavedChangesDialog"));if(!discard || !discard->property("visible").toBool()){fail("event draft close protection");return;}QMetaObject::invokeMethod(discard,"reject");}
                else if(phase==110){if(!eventEditor->property("visible").toBool() || !mainWindow->property("draftDirty").toBool()){fail("event draft cancellation retained");return;}mainWindow->setProperty("draftDirty",false);QMetaObject::invokeMethod(eventEditor,"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(2);mainWindow->resize(1440,900);}
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
                    if(mainWindow->property("sidebarWidth").toDouble()!=44 || !mainWindow->findChild<QObject*>(QStringLiteral("toggleSidebar"))->property("visible").toBool() || !capture(QStringLiteral("v1-collapsed"))){fail("recoverable collapsed navigation");return;}
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
                    if(!editor->property("visible").toBool() || !content->isVisible() || content->width()<439.5 || !capture(QStringLiteral("docked-1120"))){qCritical()<<"Layout dimensions"<<mainWindow->size()<<content->width()<<docked<<editor->property("modal");fail("docked layout");return;}
                    mainWindow->resize(880,580);
                } else if(phase==29) {
                    if(!content->isVisible() || !editor->property("visible").toBool() || editTitle->property("text").toString()!=QStringLiteral("待保留草稿") || !capture(QStringLiteral("page-880"))){fail("narrow page preserves draft");return;}
                    mainWindow->resize(1440,900);
                } else if(phase==30) {
                    if(!editor->property("visible").toBool() || editTitle->property("text").toString()!=QStringLiteral("待保留草稿") || !capture(QStringLiteral("docked-1440"))){fail("wide task navigation");return;}QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");
                } else if(phase==31) {
                    if(!guard->property("visible").toBool() || !click("cancelLeave")){fail("cancel task switch");return;}
                } else if(phase==32) {
                    if(mainWindow->property("editId").toString()!=taskId || editTitle->property("text").toString()!=QStringLiteral("待保留草稿")  ){fail("cancel retained task");return;}QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");
                } else if(phase==33) {
                    if(!guard->property("visible").toBool() || !click("discardDraft")){fail("discard task switch");return;}
                } else if(phase==34) {
                    if(editor->property("visible").toBool() || m_workspace.task(taskId).value(QStringLiteral("title")).toString()!=QStringLiteral("梳理今天的重点事项")){fail("discard zero mutation");return;}invoke("openEditor",secondId);
                    editTitle->setProperty("text",QStringLiteral("保存后导航"));invoke("changeView",QStringLiteral("inbox"));
                } else if(phase==35) {
                    if(guard->property("visible").toBool() || m_workspace.task(secondId).value("title")!=QStringLiteral("保存后导航")){fail("automatic save before navigation");return;}
                } else if(phase==36) {
                    if(m_workspace.view()!=QStringLiteral("inbox") || editor->property("visible").toBool() || m_workspace.task(secondId).value(QStringLiteral("title")).toString()!=QStringLiteral("保存后导航")){fail("saved navigation");return;}
                    m_workspace.setView(QStringLiteral("today"));invoke("openEditor",secondId);editTitle->setProperty("text",QStringLiteral("完成前修改"));
                    QMetaObject::invokeMethod(mainWindow,"changeTaskState",Q_ARG(QVariant,QVariant(secondId)),Q_ARG(QVariant,QVariant(QStringLiteral("complete"))));
                } else if(phase==37) {
                    if(guard->property("visible").toBool() || m_workspace.task(secondId).value("title")!=QStringLiteral("完成前修改")){fail("automatic save before completion");return;}
                } else if(phase==38) {
                    const auto task=m_workspace.task(secondId);
                    if(task.value(QStringLiteral("taskStatus")).toString()!=QStringLiteral("COMPLETED") || task.value(QStringLiteral("title")).toString()!=QStringLiteral("完成前修改") || !m_workspace.errorText().isEmpty()){fail("completion fresh version");return;}
                    invoke("openEditor",taskId);editTitle->setProperty("text",QStringLiteral("导航将保留"));QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");
                } else if(phase==39) {
                    if(!guard->property("visible").toBool() || !click("cancelLeave")){fail("cancel view navigation");return;}
                } else if(phase==40) {
                    if(m_workspace.view()!=QStringLiteral("today") || editTitle->property("text").toString()!=QStringLiteral("导航将保留")){fail("navigation retained context");return;}
                    QMetaObject::invokeMethod(mainWindow,"requestDiscardEditor");
                } else if(phase==41) {
                    if(!click("discardDraft")){fail("discard view navigation");return;}invoke("changeView",QStringLiteral("anytime"));
                } else if(phase==42) {
                    if(m_workspace.view()!=QStringLiteral("anytime") || editor->property("visible").toBool() || m_workspace.task(taskId).value(QStringLiteral("title")).toString()!=QStringLiteral("梳理今天的重点事项")){fail("discard navigation zero mutation");return;}
                    m_workspace.setTextScale(2.0);mainWindow->resize(1440,900);invoke("openEditor",taskId);
                } else if(phase==43) {
                    if(!editor->property("visible").toBool() || content->width()<439.5 || !capture(QStringLiteral("docked-200"))){qCritical()<<"Layout dimensions"<<mainWindow->size()<<content->width()<<docked;fail("large text docked");return;}
                    mainWindow->resize(880,580);
                } else if(phase==44) {
                    if(!content->isVisible() || !editor->property("visible").toBool() || !capture(QStringLiteral("page-200"))){fail("large text page");return;}
                    m_workspace.setTextScale(1.0);mainWindow->resize(1120,720);
                    QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(QStringLiteral("today"))),Q_ARG(QVariant,QVariant(taskId)));
                }
            }
            if(phase==270) {
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
            if(phase<437)return;
            // Occluded native windows may not receive frames to finish ListView
            // incubation. Complete the synthetic view before reporting/teardown.
            // Bound the retries: each pass only incubates for 5ms, so a platform that
            // keeps queuing delegates (macOS here) would otherwise spin until the stall
            // guard fires and the whole run is reported as timed out at phase 437.
            static int incubationPasses=0;
            if(auto* controller=m_engine.incubationController();controller && controller->incubatingObjectCount()>0) {
                controller->incubateFor(5);
                if(++incubationPasses<450){
                    // Re-entering the same phase is progress, not a stall: stamp the guard
                    // so a slow incubator is not reported as "timed out at phase 437".
                    *smokeProgressPhase=-2;*smokeProgress=inputClock->elapsed();
                    --phase;return;
                }
                qCritical()<<"Incubation still pending after"<<incubationPasses<<"passes:"<<controller->incubatingObjectCount();
            }
            incubationPasses=0;
            int visibleWindows = 0;
            for (auto* window : QGuiApplication::allWindows()) {
                if (qobject_cast<QQuickWindow*>(window) && window->isVisible()
                    && window->width() > 0 && window->height() > 0)
                    ++visibleWindows;
            }
            const int expectedWindows=(smokeProfile==QStringLiteral("desktop") || smokeProfile==QStringLiteral("calendar")) ? 3 : 2;
            const bool passed = visibleWindows == expectedWindows && !m_qmlWarnings && m_workspace.ready();
            const bool screenshotOk=screenshotPath.isEmpty() || mainWindow->grabWindow().save(screenshotPath);
            qInfo() << "QML startup: visible windows=" << visibleWindows
                    << "warnings=" << m_qmlWarnings;
            *smokePhase=phase;m_verificationPhase=phase;
            *smokeCompleted=true;
            m_verificationCompleted=passed && screenshotOk && soakSeconds==0;
            if(passed && screenshotOk && soakSeconds>0) {
                probe->stop();
                auto* soak=new SoakRunner(m_workspace,m_widget,mainWindow,soakSeconds,soakReport,[this]{return m_qmlWarnings;},&m_engine);
                soak->setCompletionCallback([this](bool success){m_verificationCompleted=success;});soak->start();return;
            }
            QCoreApplication::exit(passed && screenshotOk ? 0 : 2);
            probe->stop();
        });
        probe->start();
        // The 45s guard bounds a *stall*, not the total run: capturing every surface
        // adds synchronous readbacks, and loaded/software-rendered CI runners need
        // well over 45s for all 437 phases. Screenshot runs keep their larger budget.
        const qint64 stallBudget=screenshotPath.isEmpty() ? 45000 : 120000;
        auto* guard=new QTimer(&m_engine);
        guard->setInterval(250);
        QObject::connect(guard,&QTimer::timeout,&m_engine,[smokePhase,smokeCompleted,smokeProgress,inputClock,stallBudget]{
            if(*smokeCompleted)return;
            // start() may not have ticked yet; measure from the clock start then.
            const qint64 last=*smokeProgress;
            const qint64 stalled=last<0 ? inputClock->elapsed() : inputClock->elapsed()-last;
            if(stalled<stallBudget)return;
            qCritical()<<"UI smoke timeout at phase"<<*smokePhase;
            QCoreApplication::exit(3);
        });
        guard->start();
    }
    return true;
}

} // namespace daymark

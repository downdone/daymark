#include "AppRuntime.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QMessageBox>
#include <QQmlError>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <QDir>
#include <QStandardPaths>

namespace daymark {

bool AppRuntime::start(bool smokeTest, const QString& dataDirectory, const QString& screenshotPath)
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
    if(!smokeTest)m_tray=std::make_unique<DesktopTray>(m_widget,mainWindow);
    QObject::connect(&m_workspace,&TaskWorkspace::reminderReady,&m_engine,[this](const QString& title,const QString& message){
        if(m_tray)m_tray->notify(title,message);
    });
    QObject::connect(qApp,&QCoreApplication::aboutToQuit,&m_engine,[this]{m_widget.saveSettings();});
    QObject::connect(&m_instance,&SingleInstance::activationRequested,&m_engine,[mainWindow]{
        if(mainWindow) {mainWindow->showNormal();mainWindow->requestActivate();}
    });
    m_workspace.start(QDir(directory).filePath(QStringLiteral("daymark.sqlite")));
    QObject::connect(qGuiApp,&QGuiApplication::applicationStateChanged,&m_workspace,[this](Qt::ApplicationState state){if(state==Qt::ApplicationActive)m_workspace.refreshClock();});
    m_systemEvents=std::make_unique<SystemEventFilter>([this]{QMetaObject::invokeMethod(&m_workspace,"refreshClock",Qt::QueuedConnection);});

    if (smokeTest) {
        auto* probe=new QTimer(&m_engine);
        probe->setInterval(100);
        QObject::connect(probe,&QTimer::timeout,&m_engine,[this,probe,mainWindow,screenshotPath,phase=0,taskId=QString(),secondId=QString()]() mutable {
            if(m_workspace.busy())return;
            auto fail=[probe](const char* stage){qCritical()<<"Basic UI smoke failed at"<<stage;probe->stop();QCoreApplication::exit(2);};
            if(!m_workspace.ready()) {fail("database initialization");return;}
            auto* input=mainWindow->findChild<QObject*>(QStringLiteral("newTaskTitle"));
            auto* editTitle=mainWindow->findChild<QObject*>(QStringLiteral("editTaskTitle"));
            auto* startDate=mainWindow->findChild<QObject*>(QStringLiteral("editStartDate"));
            auto* editor=mainWindow->findChild<QObject*>(QStringLiteral("taskEditor"));
            if(!input || !editTitle || !startDate || !editor){fail("UI objects");return;}
            if(phase==0) {
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
                    if(docked || !editor->property("modal").toBool() || editTitle->property("text").toString()!=QStringLiteral("待保留草稿") || !capture(QStringLiteral("overlay-880"))){fail("narrow preserves draft");return;}
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
                    if(docked || !editor->property("modal").toBool() || !capture(QStringLiteral("overlay-200"))){fail("large text overlay");return;}
                    m_workspace.setTextScale(1.0);mainWindow->resize(1120,720);
                    QMetaObject::invokeMethod(mainWindow,"showVerificationPage",Q_ARG(QVariant,QVariant(QStringLiteral("today"))),Q_ARG(QVariant,QVariant(taskId)));
                }
            }
            ++phase;
            if(phase<78)return;
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
            QCoreApplication::exit(passed && screenshotOk ? 0 : 2);
            probe->stop();
        });
        probe->start();
        QTimer::singleShot(25000,&m_engine,[]{QCoreApplication::exit(3);});
    }
    return true;
}

} // namespace daymark

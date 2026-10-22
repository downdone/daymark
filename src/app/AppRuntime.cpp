#include "AppRuntime.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QMessageBox>
#include <QQmlError>
#include <QQuickWindow>
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
    m_engine.setInitialProperties({{QStringLiteral("widgetController"), QVariant::fromValue(&m_widget)},
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

    if (smokeTest) {
        auto* probe=new QTimer(&m_engine);
        probe->setInterval(100);
        QObject::connect(probe,&QTimer::timeout,&m_engine,[this,probe,mainWindow,screenshotPath,phase=0,taskId=QString()]() mutable {
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
            }
            ++phase;
            if(phase<26)return;
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
        QTimer::singleShot(10000,&m_engine,[]{QCoreApplication::exit(3);});
    }
    return true;
}

} // namespace daymark

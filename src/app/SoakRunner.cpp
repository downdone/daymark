#include "SoakRunner.h"
#include "models/TaskWorkspace.h"
#include "models/WidgetViewModel.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSysInfo>
#include <QThread>
#include <QDebug>

namespace daymark {
SoakRunner::SoakRunner(TaskWorkspace& workspace,WidgetViewModel& widget,QWindow* mainWindow,
                       int seconds,QString report,std::function<bool()> warnings,QObject* parent)
    :QObject(parent),m_workspace(workspace),m_widget(widget),m_main(mainWindow),m_seconds(seconds),
     m_report(std::move(report)),m_warnings(std::move(warnings))
{
    m_tick.setInterval(1000);
    connect(&m_tick,&QTimer::timeout,this,[this]{tick();});
    connect(&m_workspace,&TaskWorkspace::operationFinished,this,[this](const QString& operation,bool success){
        if(!m_finished && !success)finish(false,QStringLiteral("operation-")+operation);
    });
}
void SoakRunner::start()
{
    m_startedUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_elapsed.start();m_tick.start();
}
void SoakRunner::tick()
{
    if(m_finished)return;
    const auto now=m_elapsed.elapsed();const auto gap=now-m_lastTick;
    m_maxGap=qMax(m_maxGap,qMax<qint64>(0,gap-1000));m_lastTick=now;
    // A suspended/debug-stopped event loop is recorded, not misreported as a DB deadlock.
    if(gap>5000){++m_pauseGaps;m_lastProgress=now;}
    if(m_warnings() || !m_main || !m_workspace.ready()){finish(false,QStringLiteral("runtime-health"));return;}
    if(m_workspace.busy()) {if(now-m_lastProgress>30000)finish(false,QStringLiteral("worker-timeout"));return;}
    if(m_phase==0 && now>=qint64(m_seconds)*1000 && m_cycles>0){finish(true,QStringLiteral("completed"));return;}
    const auto task=m_taskId.isEmpty()?QVariantMap():m_workspace.task(m_taskId);
    bool accepted=true;
    switch(m_phase) {
    case 0:
        m_workspace.setView(QStringLiteral("today"));
        accepted=m_workspace.createTask(QStringLiteral("stability-probe-%1").arg(m_cycles));break;
    case 1: {
        const auto choices=m_workspace.eventTaskChoices(QStringLiteral("stability-probe-%1").arg(m_cycles));
        if(choices.size()!=1){finish(false,QStringLiteral("created-task"));return;}
        const auto created=choices.first().toMap();m_taskId=created.value(QStringLiteral("taskId")).toString();
        accepted=m_workspace.saveTaskWithDetails(m_taskId,created.value(QStringLiteral("rowVersion")).toLongLong(),
            QStringLiteral("stability-probe-%1").arg(m_cycles),QStringLiteral("synthetic"),QStringLiteral("ACTIVE"),m_workspace.dateOffset(0),m_workspace.dateOffset(2),false,
            {{"important",true},{"schedule",QVariantMap{{"allDay",true},{"startDate",m_workspace.dateOffset(0)},{"endDate",m_workspace.dateOffset(2)}}}});break;
    }
    case 2:
        if(!task.value(QStringLiteral("important")).toBool() || task.value(QStringLiteral("schedule")).toMap().isEmpty()){finish(false,QStringLiteral("complete-draft"));return;}
        accepted=m_workspace.changeState(m_taskId,task.value(QStringLiteral("rowVersion")).toLongLong(),QStringLiteral("complete"));break;
    case 3:
        if(task.value(QStringLiteral("taskStatus")).toString()!=QStringLiteral("COMPLETED")){finish(false,QStringLiteral("completed-state"));return;}
        accepted=m_workspace.changeState(m_taskId,task.value(QStringLiteral("rowVersion")).toLongLong(),QStringLiteral("reopen"));break;
    case 4:accepted=m_workspace.changeState(m_taskId,task.value(QStringLiteral("rowVersion")).toLongLong(),QStringLiteral("delete"));break;
    case 5:
        if(!task.isEmpty()){finish(false,QStringLiteral("soft-delete"));return;}
        accepted=m_workspace.undoDelete();break;
    case 6:
        if(task.value(QStringLiteral("taskStatus")).toString()!=QStringLiteral("OPEN")){finish(false,QStringLiteral("restore-state"));return;}
        if(m_cycles%60==0)accepted=m_workspace.createBackup();break;
    case 7:accepted=m_workspace.verifyStorage();if(accepted)++m_integrityChecks;break;
    case 8:m_workspace.reload();break;
    case 9:
        if(!task.value(QStringLiteral("important")).toBool() || task.value(QStringLiteral("schedule")).toMap().isEmpty()){finish(false,QStringLiteral("reload-content"));return;}
        accepted=m_workspace.changeState(m_taskId,task.value(QStringLiteral("rowVersion")).toLongLong(),QStringLiteral("delete"));break;
    case 10: {
        if(!task.isEmpty()){finish(false,QStringLiteral("cycle-cleanup"));return;}
        const QStringList views={"inbox","today","upcoming","anytime","someday","logbook","month","week"};
        m_workspace.setView(views[m_cycles%views.size()]);m_workspace.refreshClock();
        accepted=m_widget.setMini(m_cycles%2==0) && m_widget.setVisible(m_cycles%3!=0);
        if(accepted){m_widget.normalizeGeometry();m_widget.saveSettings();}
        ++m_cycles;m_taskId.clear();break;
    }
    }
    if(!accepted){finish(false,QStringLiteral("command-rejected-%1").arg(m_phase));return;}
    m_phase=(m_phase+1)%11;m_lastProgress=now;
}
void SoakRunner::finish(bool passed,const QString& stage)
{
    if(m_finished)return;m_finished=true;m_tick.stop();
    QJsonObject report{{"format",1},{"passed",passed},{"stage",stage},{"version",QCoreApplication::applicationVersion()},
        {"qt",QString::fromLatin1(qVersion())},{"platform",QGuiApplication::platformName()},
        {"os",QSysInfo::prettyProductName()},{"architecture",QSysInfo::currentCpuArchitecture()},
        {"logicalProcessors",QThread::idealThreadCount()},{"startedUtc",m_startedUtc},
        {"endedUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"requestedSeconds",m_seconds},{"elapsedSeconds",m_elapsed.elapsed()/1000.0},
        {"completedCycles",m_cycles},{"integrityChecks",m_integrityChecks},
        {"maxEventLoopDelayMs",double(m_maxGap)},{"observedPauseGaps",m_pauseGaps},
        {"scope",QStringLiteral("isolated synthetic continuous process; OS sleep and multi-screen require manual evidence")},
        {"userDataAccessed",false},{"manualSleepMultiScreenVerified",false}};
    QSaveFile output(m_report);
    const auto bytes=QJsonDocument(report).toJson();
    if(!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit())passed=false;
    qInfo()<<"Stability run"<<(passed?"PASS":"FAIL")<<"cycles="<<m_cycles<<"stage="<<stage;
    QCoreApplication::exit(passed?0:2);
}
}

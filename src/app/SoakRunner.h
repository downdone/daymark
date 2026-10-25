#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QPointer>
#include <QWindow>
#include <functional>

namespace daymark {
class TaskWorkspace;
class WidgetViewModel;
// Only constructed after synthetic smoke; never used with a user's data directory.
class SoakRunner final:public QObject {
public:
    SoakRunner(TaskWorkspace& workspace,WidgetViewModel& widget,QWindow* mainWindow,
               int seconds,QString report,std::function<bool()> warnings,QObject* parent);
    void start();
    void setCompletionCallback(std::function<void(bool)> callback){m_completed=std::move(callback);}
private:
    void tick();
    void finish(bool passed,const QString& stage);
    TaskWorkspace& m_workspace;
    WidgetViewModel& m_widget;
    QPointer<QWindow> m_main;
    int m_seconds,m_phase=0,m_cycles=0,m_integrityChecks=0,m_pauseGaps=0;
    qint64 m_lastTick=0,m_lastProgress=0,m_maxGap=0;
    QString m_report,m_taskId,m_startedUtc;
    std::function<bool()> m_warnings;
    std::function<void(bool)> m_completed;
    QElapsedTimer m_elapsed;
    QTimer m_tick;
    bool m_finished=false;
};
}

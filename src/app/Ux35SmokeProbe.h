#pragma once
#include <QElapsedTimer>
#include <QPointer>
#include <QVariantMap>
class QQuickWindow;
namespace daymark {
class TaskWorkspace;
// Independent fixture for continuous scrolling. Keeps the legacy smoke's 45s budget.
class Ux35SmokeProbe final {
public:
    Ux35SmokeProbe(TaskWorkspace& workspace,QQuickWindow* window,QString screenshot);
    QString advance();
private:
    QObject* find(const QString& name) const;
    bool invoke(QObject* object,const char* method,const QVariant& value={});
    bool capture(const QString& suffix)const;
    void click(const QPointF& at);
    TaskWorkspace& m_workspace;
    QPointer<QQuickWindow> m_window;
    QString m_screenshot,m_scope,m_task,m_windowAnchor;
    int m_stage=0;
    bool m_retriedDraft=false;
    QElapsedTimer m_wait,m_inputClock;
    quint64 m_inputSequence=0;
    QVariantMap m_before;
};
}

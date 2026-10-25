#pragma once
#include <QPointer>
#include <QVariantMap>
#include <QElapsedTimer>
class QQuickWindow;
namespace daymark {
class TaskWorkspace;
class Ux34SmokeProbe final {
public:
    Ux34SmokeProbe(TaskWorkspace& workspace,QQuickWindow* window,QString screenshot);
    // Empty: waiting. DONE: complete. Other text: failure.
    QString advance();
private:
    QObject* find(const char* name) const;
    bool invoke(QObject* object,const char* method,const QVariant& argument={});
    bool capture(const QString& suffix,QQuickWindow* window=nullptr) const;
    TaskWorkspace& m_workspace;
    QPointer<QQuickWindow> m_window;
    QString m_screenshot,m_scope,m_first,m_second,m_unscheduled;
    int m_stage=0;
    QElapsedTimer m_wait,m_inputClock;
    quint64 m_inputSequence=0;
    QVariantMap m_before;
};
}

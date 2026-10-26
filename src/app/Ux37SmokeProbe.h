#pragma once
#include <QElapsedTimer>
#include <QPointer>
#include <QStringList>
#include <QVariantMap>
class QQuickWindow;
class QQuickItem;
namespace daymark {
class TaskWorkspace;
class Ux37SmokeProbe final {
public:
    Ux37SmokeProbe(TaskWorkspace& workspace,QQuickWindow* window,QString screenshot);
    QString advance();
private:
    QObject* find(const QString& name) const;
    void click(QQuickItem* item,Qt::KeyboardModifiers modifiers=Qt::NoModifier,Qt::MouseButton button=Qt::LeftButton);
    void hover(QQuickItem* item);
    void key(QQuickItem* target,int code,Qt::KeyboardModifiers modifiers=Qt::NoModifier);
    bool capture(const QString& name) const;
    TaskWorkspace& m_workspace;
    QPointer<QQuickWindow> m_window;
    QString m_screenshot,m_parent;
    QStringList m_ids;
    QVariantMap m_columns;
    int m_stage=0;
    QElapsedTimer m_wait,m_clock,m_presentClock;
    qint64 m_panelFrameMs=-1;
    bool m_awaitingPanelFrame=false;
    quint64 m_sequence=0;
};
}

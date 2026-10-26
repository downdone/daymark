#pragma once
#include <QElapsedTimer>
#include <QPointer>
#include <QVariantMap>
#include <QPointF>
#include <QEvent>
class QQuickWindow;
class QQuickItem;
namespace daymark {
class TaskWorkspace;
class Ux39SmokeProbe final {
public:
    Ux39SmokeProbe(TaskWorkspace&,QQuickWindow*,QString);
    QString advance();
private:
    QObject* find(QObject*,const QString&) const;
    void click(QQuickItem*,bool twice=false);
    void key(QQuickWindow*,int,Qt::KeyboardModifiers=Qt::NoModifier);
    void pointer(QEvent::Type,const QPointF&);
    bool capture(const QString&,QQuickWindow* = nullptr) const;
    TaskWorkspace& m_workspace;
    QPointer<QQuickWindow> m_window;
    QString m_screenshot,m_task,m_savedView;
    double m_viewMinute=0;
    int m_stage=0;
    QElapsedTimer m_wait,m_clock;
    quint64 m_sequence=0;
    QVariantMap m_before;
    QPointF m_dragOrigin;
    QPointer<QQuickItem> m_yearDay;
};
}

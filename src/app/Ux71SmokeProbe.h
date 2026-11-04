#pragma once
#include <QElapsedTimer>
#include <QPointer>
#include <QString>
class QQuickWindow;
class QQuickItem;
namespace daymark {
class TaskWorkspace;
class WidgetViewModel;
class Ux71SmokeProbe final {
public:
    Ux71SmokeProbe(TaskWorkspace&,WidgetViewModel&,QQuickWindow*,QQuickWindow*,QString);
    QString advance();
    int verificationStep() const {return m_stage;}
private:
    QObject* find(QObject*,const QString&) const;
    void click(QQuickItem*,QQuickWindow* receiver=nullptr);
    bool capture(QQuickWindow*,const QString&) const;
    TaskWorkspace& m_workspace;
    WidgetViewModel& m_controller;
    QPointer<QQuickWindow> m_main,m_widget;
    QString m_screenshot;
    QElapsedTimer m_wait,m_clock;
    int m_stage=0;
    double m_beforeY=0,m_opacity=-1;
    bool m_flickered=false,m_leaked=false;
};
}

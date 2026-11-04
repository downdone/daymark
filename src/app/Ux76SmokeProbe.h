#pragma once
#include <QObject>
#include <QPointer>
#include <QElapsedTimer>
#include <QStringList>
class QQuickWindow;
class QQuickItem;
namespace daymark {
class TaskWorkspace;
class WidgetViewModel;
class Ux76SmokeProbe final : public QObject {
public:
    Ux76SmokeProbe(TaskWorkspace&,WidgetViewModel&,QQuickWindow*,QQuickWindow*,QString);
    QString advance();
    int verificationStep()const{return m_cycle*100+m_stage;}
private:
    QObject* find(const QString&)const;
    void pointer(QQuickItem*,Qt::MouseButton);
    void checkMenu();
    TaskWorkspace& m_workspace;
    WidgetViewModel& m_controller;
    QPointer<QQuickWindow> m_main,m_widget;
    QString m_screenshot,m_error;
    QStringList m_tasks;
    QElapsedTimer m_wait;
    int m_stage=0,m_cycle=0;
    bool m_native=false,m_observed=false;
};
}

#pragma once
#include <QElapsedTimer>
#include <QPointer>
#include <QString>
class QQuickItem;
class QQuickWindow;
namespace daymark {
class TaskWorkspace;
class WidgetViewModel;
class Ux38SmokeProbe final {
public:
    Ux38SmokeProbe(TaskWorkspace&,WidgetViewModel&,QQuickWindow*,QQuickWindow*,QString);
    QString advance();
private:
    QObject* find(QObject* root,const QString& name) const;
    void click(const QString& name,QObject* scope=nullptr);
    void expandWithKey(int key);
    void hover(QObject*,const QString&);
    bool capture(QQuickWindow*,const QString& name) const;
    bool layout() const;
    TaskWorkspace& m_workspace;
    WidgetViewModel& m_controller;
    QPointer<QQuickWindow> m_main,m_widget;
    QPointer<QQuickItem> m_completedRow;
    QString m_screenshot,m_task;
    QString m_shortCategory,m_longCategory;
    int m_stage=0,m_count=0;
    QElapsedTimer m_wait;
};
}

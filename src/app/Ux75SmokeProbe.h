#pragma once
#include <QObject>
#include <QPointer>
#include <QElapsedTimer>
#include <QImage>
#include <QRect>
class QQuickWindow;
class QQuickItem;
namespace daymark {
class TaskWorkspace;class WidgetViewModel;
class Ux75SmokeProbe final : public QObject {
public:
    Ux75SmokeProbe(TaskWorkspace&,WidgetViewModel&,QQuickWindow*,QQuickWindow*,QString);
    QString advance();
    int verificationStep()const{return m_stage+m_cycle*100+m_target*10;}
private:
    QObject* find(QObject*,const QString&)const;
    TaskWorkspace& m_workspace;WidgetViewModel& m_controller;
    QPointer<QQuickWindow> m_main,m_widget;
    QString m_screenshot;QElapsedTimer m_wait;
    QRect m_canvas;QImage m_ballFrame;
    int m_syncedFrames=0,m_swappedFrames=0,m_endedFrames=0;
    int m_stage=0,m_cycle=0,m_target=0,m_samples=0,m_geometryEvents=0,m_geometryBaseline=0,m_intermediateFrames=0;
    bool m_monitor=false,m_emptyMask=false,m_lostGraph=false;
};
}

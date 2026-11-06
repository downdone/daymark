#pragma once
#include <QString>
class QQuickWindow;
namespace daymark {
class TaskWorkspace;class WidgetViewModel;
class TimeInputSmokeProbe final {
public:
    TimeInputSmokeProbe(TaskWorkspace&,WidgetViewModel&,QQuickWindow*,QQuickWindow*,QString);
    QString advance();
private:
    TaskWorkspace& w;WidgetViewModel& controller;QQuickWindow* mainWindow;QQuickWindow* widget;
    QString screenshot;int phase=0;int before=0;int popupCycle=0;
};
}

#pragma once
#include <QPointer>
#include <QString>
class QQuickWindow;
class QQuickItem;
namespace daymark {
class TaskWorkspace;
class WindowChromeController;
class ChromeSmokeProbe final {
  public:
    ChromeSmokeProbe(TaskWorkspace &, WindowChromeController &, QQuickWindow *,
                     QString screenshot);
    QString advance();

  private:
    QObject *find(QObject *root, const QString &name) const;
    bool pressKey(QQuickItem *item, int key = Qt::Key_Space);
    bool capture(QQuickWindow *target, const QString &suffix);
    TaskWorkspace &workspace;
    WindowChromeController &controller;
    QQuickWindow *window;
    QPointer<QQuickItem> title;
    QString taskId, screenshot;
    int phase = 0;
};
} // namespace daymark

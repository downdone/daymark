#pragma once
#include <QPointer>
#include <QHash>
#include <QRectF>
#include <QString>
class QQuickWindow;
class QQuickItem;
namespace daymark {
class TaskWorkspace;
class WindowChromeController;
class ChromeSmokeProbe final {
  public:
    ChromeSmokeProbe(TaskWorkspace &, WindowChromeController &, QQuickWindow *,
                     QString screenshot, bool layoutOnly = false);
    QString advance();
    int intervalMs() const { return phase < 18 ? 240 : 80; }

  private:
    QObject *find(QObject *root, const QString &name) const;
    bool pressKey(QQuickItem *item, int key = Qt::Key_Space);
    bool capture(QQuickWindow *target, const QString &suffix);
    QString advanceLayout(QQuickWindow *settings, QQuickWindow *account);
    QString compareLayout(QObject *root, const QStringList &names, bool reset);
    TaskWorkspace &workspace;
    WindowChromeController &controller;
    QQuickWindow *window;
    QPointer<QQuickItem> title;
    QString taskId, screenshot;
    QString categoryId, tagId;
    QHash<QString, QRectF> layoutBaseline;
    QPointer<QObject> updateStub;
    int phase = 0;
};
} // namespace daymark

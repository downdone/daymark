#pragma once
#include <QPointer>
#include <QString>
#include <QVariant>
class QQmlApplicationEngine;
class QQuickWindow;
class QQuickItem;
namespace daymark {
class TaskWorkspace;
class PickerLifecycleProbe final {
public:
  PickerLifecycleProbe(QQmlApplicationEngine &, TaskWorkspace &,
                       QQuickWindow *, const QString &screenshotPath);
  ~PickerLifecycleProbe();
  QString advance();

private:
  QQmlApplicationEngine &engine;
  TaskWorkspace &workspace;
  QQuickWindow *window;
  const QString screenshotPath;
  QPointer<QQuickWindow> timingWindow;
  QPointer<QQuickItem> date, time, timing;
  QPointer<QObject> catalog;
  QVariant repeatPresets;
  qreal compactCatalogHeight = 0;
  int phase = 0;
};
} // namespace daymark

#pragma once
#include <QPointer>
#include <QString>
class QQmlApplicationEngine;
class QQuickWindow;
class QQuickItem;
namespace daymark {
class TaskWorkspace;
class PickerLifecycleProbe final {
public:
  PickerLifecycleProbe(QQmlApplicationEngine &, TaskWorkspace &,
                       QQuickWindow *);
  ~PickerLifecycleProbe();
  QString advance();

private:
  QQmlApplicationEngine &engine;
  TaskWorkspace &workspace;
  QQuickWindow *window;
  QPointer<QQuickItem> date, time;
  QPointer<QObject> catalog;
  qreal compactCatalogHeight = 0;
  int phase = 0;
};
} // namespace daymark

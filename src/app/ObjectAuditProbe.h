#pragma once
#include <QElapsedTimer>
#include <QJsonArray>
#include <QString>
class QQmlApplicationEngine;
class QQuickWindow;
namespace daymark {
class TaskWorkspace;
class WidgetViewModel;
class ObjectAuditProbe final {
public:
  ObjectAuditProbe(QQmlApplicationEngine &, TaskWorkspace &, WidgetViewModel &,
                   QQuickWindow *, QString report);
  static bool seedFixture(const QString &database);
  QString advance();

private:
  void capture(const QString &stage);
  QQmlApplicationEngine &engine;
  TaskWorkspace &workspace;
  WidgetViewModel &controller;
  QQuickWindow *mainWindow;
  QString destination;
  QJsonArray records;
  QElapsedTimer settle;
  QElapsedTimer runClock;
  int phase = 0, cycle = 0;
};
} // namespace daymark

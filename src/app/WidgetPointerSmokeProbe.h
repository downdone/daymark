#pragma once
#include <QPointF>
#include <QString>
class QQuickWindow;
namespace daymark {
class WidgetViewModel;
class WidgetPointerSmokeProbe final {
public:
  WidgetPointerSmokeProbe(WidgetViewModel &value, QQuickWindow *window)
      : controller(value), widget(window) {}
  QString advance();

private:
  WidgetViewModel &controller;
  QQuickWindow *widget;
  int phase = 0;
  QPointF press;
  QPoint origin;
};
} // namespace daymark

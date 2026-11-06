#include "WidgetPointerSmokeProbe.h"
#include "models/WidgetViewModel.h"
#include <QCoreApplication>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
namespace daymark {
namespace {
QQuickItem *find(QQuickWindow *window, const QString &name) {
  QList<QQuickItem *> queue{window->contentItem()};
  while (!queue.isEmpty()) {
    auto *item = queue.takeFirst();
    if (item->objectName() == name)
      return item;
    queue.append(item->childItems());
  }
  return nullptr;
}
void mouse(QQuickWindow *window, QEvent::Type type, const QPointF &global,
           Qt::MouseButton button, Qt::MouseButtons buttons) {
  const auto local = global - QPointF(window->position());
  QMouseEvent event(type, local, local, global, button, buttons,
                    Qt::NoModifier);
  static ulong stamp = 100;
  stamp += 250;
  event.setTimestamp(stamp);
  QCoreApplication::sendEvent(window, &event);
}
} // namespace
QString WidgetPointerSmokeProbe::advance() {
  auto fail = [&](const QString &text) {
    return QStringLiteral("WID08 phase %1: %2").arg(phase).arg(text);
  };
  if (phase == 0) {
    controller.setVisible(true);
    controller.setLocked(false);
    controller.setAlwaysOnTop(true);
    controller.selectForm(1);
  } else if (phase == 1) {
    auto *grip = find(widget, "widgetMiniGrip");
    if (!grip || !grip->isVisible())
      return fail("mini grip missing");
    press = grip->mapToGlobal(QPointF(grip->width() / 2, grip->height() / 2));
    mouse(widget, QEvent::MouseButtonPress, press, Qt::LeftButton,
          Qt::LeftButton);
    mouse(widget, QEvent::MouseButtonRelease, press, Qt::LeftButton,
          Qt::NoButton);
  } else if (phase == 2) {
    if (controller.effectiveForm() != 0)
      return fail("resident Mini sun did not open panel");
    controller.selectForm(1);
  } else if (phase == 3) {
    auto *grip = find(widget, "widgetMiniGrip");
    if (!grip)
      return fail("mini grip");
    press = grip->mapToGlobal(QPointF(grip->width() / 2, grip->height() / 2));
    origin = widget->position();
    mouse(widget, QEvent::MouseButtonPress, press, Qt::LeftButton,
          Qt::LeftButton);
  } else if (phase == 4)
    mouse(widget, QEvent::MouseMove, press + QPointF(25, 0), Qt::NoButton,
          Qt::LeftButton);
  else if (phase == 5)
    mouse(widget, QEvent::MouseMove, press + QPointF(65, 35), Qt::NoButton,
          Qt::LeftButton);
  else if (phase == 6) {
    mouse(widget, QEvent::MouseButtonRelease, press + QPointF(65, 35),
          Qt::LeftButton, Qt::NoButton);
    if (widget->position() == origin)
      return fail("Mini DragHandler did not move real window");
    if (!controller.alwaysOnTop())
      return fail("drag lost topmost");
    if (controller.effectiveForm() != 1)
      return fail("drag activated click");
    controller.selectForm(0);
  } else if (phase == 7) {
    auto *grip = find(widget, "widgetHeaderGrip");
    if (!grip)
      return fail("panel grip missing");
    press = grip->mapToGlobal(QPointF(grip->width() / 2, grip->height() / 2));
    origin = widget->position();
    mouse(widget, QEvent::MouseButtonPress, press, Qt::LeftButton,
          Qt::LeftButton);
  } else if (phase == 8)
    mouse(widget, QEvent::MouseMove, press + QPointF(25, 0), Qt::NoButton,
          Qt::LeftButton);
  else if (phase == 9)
    mouse(widget, QEvent::MouseMove, press + QPointF(55, 25), Qt::NoButton,
          Qt::LeftButton);
  else if (phase == 10) {
    mouse(widget, QEvent::MouseButtonRelease, press + QPointF(55, 25),
          Qt::LeftButton, Qt::NoButton);
    if (widget->position() == origin || !controller.alwaysOnTop())
      return fail("pinned panel did not drag");
  } else if (phase == 11) {
    if (!controller.beginPointerMove(QPointF(100, 100)) ||
        !controller.movePointer(QPointF(120, 130)))
      return fail("panel pointer movement");
    controller.setLocked(true);
    if (controller.movePointer(QPointF(130, 140)))
      return fail("lock left active movement");
    controller.setLocked(false);
    if (!controller.beginPointerMove(QPointF(200, 200)) ||
        !controller.movePointer(QPointF(210, 210)))
      return fail("unlock stuck");
    controller.finishMove();
    return "DONE";
  }
  ++phase;
  return {};
}
} // namespace daymark

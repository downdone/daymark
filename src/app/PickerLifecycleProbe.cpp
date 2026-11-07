#include "PickerLifecycleProbe.h"
#include "models/TaskWorkspace.h"
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
namespace daymark {
PickerLifecycleProbe::PickerLifecycleProbe(QQmlApplicationEngine &e,
                                           TaskWorkspace &w, QQuickWindow *v)
    : engine(e), workspace(w), window(v) {}
PickerLifecycleProbe::~PickerLifecycleProbe() {
  delete date.data();
  delete time.data();
}
QString PickerLifecycleProbe::advance() {
  if (!workspace.ready() || workspace.busy())
    return {};
  auto fail = [&](const char *reason) {
    return QStringLiteral("OBJ01 phase %1: %2")
        .arg(phase)
        .arg(QString::fromLatin1(reason));
  };
  auto invoke = [](QObject *item, const char *method) {
    return QMetaObject::invokeMethod(item, method);
  };
  if (phase == 0) {
    auto create = [&](const char *name) {
      QQmlComponent component(&engine);
      component.loadFromModule("Daymark", QString::fromLatin1(name));
      auto *object = component.createWithInitialProperties(
          QVariantMap{{"width", 260}, {"text", QStringLiteral("")}});
      auto *item = qobject_cast<QQuickItem *>(object);
      if (item) {
        item->setParent(window->contentItem());
        item->setParentItem(window->contentItem());
        item->setY(name[0] == 'D' ? 120 : 220);
      } else
        delete object;
      return item;
    };
    QQmlComponent component(&engine);
    component.loadFromModule("Daymark", "DateField");
    date = qobject_cast<QQuickItem *>(component.createWithInitialProperties(
        {{"workspace", QVariant::fromValue(&workspace)},
         {"width", 260},
         {"text", workspace.dateOffset(0)}}));
    if (!date)
      return fail("date creation");
    date->setParent(window->contentItem());
    date->setParentItem(window->contentItem());
    date->setY(120);
    time = create("TimeField");
    if (!time)
      return fail("time creation");
    time->setProperty("text", "23:45");
  } else if (phase == 1) {
    if (date->property("pickerContentLoaded").toBool() ||
        time->property("pickerContentLoaded").toBool())
      return fail("cold content constructed");
    if (!invoke(date, "openPicker"))
      return fail("date open");
  } else if (phase == 2) {
    if (!date->property("pickerContentLoaded").toBool() ||
        !date->property("pickerVisible").toBool())
      return fail("date not ready");
    if (!QMetaObject::invokeMethod(
            date, "chooseDate",
            Q_ARG(QVariant, QVariant(workspace.dateOffset(2)))))
      return fail("date choose");
  } else if (phase == 3) {
    if (date->property("pickerContentLoaded").toBool() ||
        date->property("text").toString() != workspace.dateOffset(2))
      return fail("date close/state");
    date->setProperty("inlineMode", true);
    invoke(date, "openPicker");
  } else if (phase == 4) {
    if (date->property("pickerContentLoaded").toBool() ||
        !date->property("inlineExpanded").toBool())
      return fail("inline created fallback");
    invoke(date, "closePicker");
    invoke(time, "openPicker");
  } else if (phase == 5) {
    auto *hours = time->findChild<QObject *>("pickerHour");
    auto *minutes = time->findChild<QObject *>("pickerMinute");
    if (!time->property("pickerContentLoaded").toBool() || !hours || !minutes ||
        hours->property("value").toInt() != 23 ||
        minutes->property("value").toInt() != 45)
      return fail("time restored");
    invoke(time, "closePicker");
  } else if (phase == 6) {
    if (time->property("pickerContentLoaded").toBool() ||
        time->property("text") != "23:45")
      return fail("time cancel");
    invoke(time, "openPicker");
  } else if (phase == 7) {
    if (!QMetaObject::invokeMethod(time, "chooseTime",
                                   Q_ARG(QVariant, QVariant(8)),
                                   Q_ARG(QVariant, QVariant(5))))
      return fail("time choose");
  } else if (phase == 8) {
    if (time->property("pickerContentLoaded").toBool() ||
        time->property("text") != "08:05")
      return fail("time committed/reset");
    time->setProperty("inlineMode", true);
    invoke(time, "openPicker");
  } else if (phase == 9) {
    if (time->property("pickerContentLoaded").toBool() ||
        !time->property("inlineExpanded").toBool())
      return fail("inline time fallback");
    invoke(time, "closePicker");
    workspace.setTextScale(2);
    date->setProperty("inlineMode", false);
    invoke(date, "openPicker");
  } else if (phase == 10) {
    if (!date->property("pickerContentLoaded").toBool())
      return fail("large date open");
    invoke(date, "closePicker");
  } else if (phase == 11) {
    if (date->property("pickerContentLoaded").toBool())
      return fail("large date retained");
    return "DONE";
  }
  ++phase;
  return {};
}
} // namespace daymark

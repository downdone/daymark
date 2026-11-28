#include "PickerLifecycleProbe.h"
#include "models/TaskWorkspace.h"
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
namespace daymark {
PickerLifecycleProbe::PickerLifecycleProbe(QQmlApplicationEngine &e,
                                           TaskWorkspace &w, QQuickWindow *v,
                                           const QString &path)
    : engine(e), workspace(w), window(v), screenshotPath(path) {}
PickerLifecycleProbe::~PickerLifecycleProbe() {
  delete date.data();
  delete time.data();
  delete catalog.data();
  delete timing.data();
  delete timingWindow.data();
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
    workspace.setTextScale(1);
    QQmlComponent component(&engine);
    component.loadFromModule("Daymark", "CatalogPicker");
    catalog = component.createWithInitialProperties(
        {{"parent", QVariant::fromValue(window->contentItem())},
         {"heading", QStringLiteral("分类")},
         {"options", QVariantList{
             QVariantMap{{"id", "a"}, {"title", "Alpha"}},
             QVariantMap{{"id", "b"}, {"title", "Beta"}}}}});
    if (!catalog)
      return fail("catalog creation");
    catalog->setParent(window->contentItem());
    invoke(catalog, "open");
  } else if (phase == 12) {
    compactCatalogHeight = catalog->property("height").toReal();
    if (!catalog->property("visible").toBool() || compactCatalogHeight <= 0 ||
        compactCatalogHeight >= 220)
      return fail("short catalog should use natural height");
    auto *search = catalog->findChild<QObject *>("catalogPickerSearch");
    if (!search)
      return fail("catalog search missing");
    search->setProperty("text", "missing");
  } else if (phase == 13) {
    if (catalog->property("height").toReal() >= compactCatalogHeight)
      return fail("filtered empty catalog retains blank rows");
    QVariantList options;
    for (int i = 0; i < 20; ++i)
      options.append(QVariantMap{{"id", QString::number(i)},
                                 {"title", QStringLiteral("Category %1").arg(i)}});
    catalog->setProperty("options", options);
    catalog->findChild<QObject *>("catalogPickerSearch")->setProperty("text", "");
  } else if (phase == 14) {
    const auto height = catalog->property("height").toReal();
    if (height <= compactCatalogHeight || height > window->height() - 16 ||
        height > catalog->property("heightLimit").toReal())
      return fail("long catalog should grow within work area");
    workspace.setTextScale(2);
  } else if (phase == 15) {
    const auto height = catalog->property("height").toReal();
    if (height <= 0 || height > window->height() - 16 ||
        height > catalog->property("heightLimit").toReal())
      return fail("large catalog should remain bounded");
    invoke(catalog, "close");
  } else if (phase == 16) {
    if (catalog->property("visible").toBool() ||
        time->property("text") != "08:05")
      return fail("catalog close changes unrelated field");
    catalog->setProperty("multiple", true);
    catalog->setProperty("staged", true);
    catalog->setProperty("selectedIds", QVariantList{QStringLiteral("0")});
    invoke(catalog, "open");
    if (!QMetaObject::invokeMethod(catalog, "choose",
                                  Q_ARG(QVariant, QVariant("tag")),
                                  Q_ARG(QVariant, QVariant("1")),
                                  Q_ARG(QVariant, QVariant(true))))
      return fail("staged tag selection");
    if (catalog->property("workingIds").toList().size() != 2 ||
        catalog->property("selectedIds").toList().size() != 1)
      return fail("tag choice must remain in picker draft");
    invoke(catalog, "close");
    invoke(catalog, "open");
    if (catalog->property("workingIds").toList().size() != 1)
      return fail("cancelled tag choice leaked on reopen");
    invoke(catalog, "close");
    workspace.setTextScale(1);
    QQmlComponent component(&engine);
    component.loadFromModule("Daymark", "TaskTimingFields");
    timing = qobject_cast<QQuickItem *>(component.createWithInitialProperties(
        {{"workspace", QVariant::fromValue(&workspace)},
         {"objectPrefix", "probe"}, {"width", 400}, {"compactPresentation", true}}));
    if (!timing)
      return fail("task timing fields creation");
    const auto area = window->screen()->availableGeometry();
    timingWindow = new QQuickWindow;
    timingWindow->setScreen(window->screen());
    const bool headless = QGuiApplication::platformName() == "offscreen";
    timingWindow->setGeometry(area.x() + 24, area.bottom() - (headless ? 428 : 262),
                              480, headless ? 400 : 240);
    timing->setParent(timingWindow->contentItem());
    timing->setParentItem(timingWindow->contentItem());
    timingWindow->show();
  } else if (phase >= 17) {
    auto *choice = timing->findChild<QQuickItem *>("probeRepeatChoice");
    auto *popup = choice ? choice->property("popup").value<QObject *>() : nullptr;
    auto *list = popup ? qobject_cast<QQuickItem *>(popup->property("contentItem").value<QObject *>()) : nullptr;
    if (!list)
      return fail("repeat options missing");
    auto option = [list](const QString &name) -> QQuickItem * {
      const auto *content = list->property("contentItem").value<QQuickItem *>();
      if (content) {
        for (auto *row : content->childItems()) {
          if (row->objectName() == name)
            return row;
        }
      }
      return nullptr;
    };
    auto keyClick = [this](int key) {
      QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
      QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
      QGuiApplication::sendEvent(timingWindow, &press);
      QGuiApplication::sendEvent(timingWindow, &release);
    };

    if (phase == 17) {
      if (list->property("count").toInt() != 0)
        return fail("repeat options should load when opened");
      repeatPresets = choice->property("model");
      choice->forceActiveFocus(Qt::TabFocusReason);
      keyClick(Qt::Key_Space);
    } else if (phase == 18 || phase == 19) {
      const auto *last = option(QStringLiteral("probeRepeatChoice-option-5"));
      if (!popup->property("visible").toBool() || list->property("count").toInt() != 6 ||
          !last || qAbs(list->height() - last->y() - last->height()) > 1 ||
          list->property("interactive").toBool())
        return fail("six repeat options should fit with no trailing space or scrolling") +
               QStringLiteral(" (open=%1, count=%2, viewport=%3, last bottom=%4, scroll=%5)")
                   .arg(popup->property("visible").toBool())
                   .arg(list->property("count").toInt())
                   .arg(list->height())
                   .arg(last ? last->y() + last->height() : -1)
                   .arg(list->property("interactive").toBool());
      if (phase == 18) {
        workspace.setTextScale(2);
        workspace.setThemeMode(2);
      } else {
        workspace.setTextScale(1);
        QStringList options;
        for (int i = 0; i < 14; ++i)
          options.append(QStringLiteral("Option %1").arg(i + 1));
        choice->setProperty("model", options);
      }
    } else if (phase == 20) {
      const auto platform = QGuiApplication::platformName();
      if (platform != "offscreen" && !platform.startsWith("wayland")) {
        const auto area = timingWindow->screen()->availableGeometry();
        const auto top = list->mapToGlobal(QPointF(0, 0));
        const auto *last = option(QStringLiteral("probeRepeatChoice-option-13"));
        if (list->property("count").toInt() != 14 || !last ||
            list->window() == timingWindow || list->height() <= 320 ||
            qAbs(list->height() - last->y() - last->height()) > 1 ||
            list->property("interactive").toBool() || top.y() < area.y() ||
            top.y() + list->height() > choice->mapToGlobal(QPointF(0, 0)).y())
          return fail("long options should fit above the anchor beyond the host window");
        if (!screenshotPath.isEmpty() &&
            !list->window()->grabWindow().save(screenshotPath + ".dropdown-full.png"))
          return fail("full dropdown screenshot");
      }
      workspace.setTextScale(2);
      QStringList options;
      for (int i = 0; i < 200; ++i)
        options.append(QStringLiteral("Option %1").arg(i + 1));
      choice->setProperty("model", options);
    } else if (phase == 21) {
      const auto area = timingWindow->screen()->availableGeometry();
      const auto top = list->mapToGlobal(QPointF(0, 0));
      if (list->property("count").toInt() != 200 ||
          !list->property("interactive").toBool() ||
          popup->property("height").toReal() > area.height() ||
          top.y() < area.y() || top.y() + list->height() > area.bottom() + 1)
        return fail("options taller than the screen should scroll within its work area");
      keyClick(Qt::Key_End);
    } else if (phase == 22) {
      const auto *last = option(QStringLiteral("probeRepeatChoice-option-199"));
      if (choice->property("highlightedIndex").toInt() != 199 || !last ||
          last->mapToItem(list, QPointF(0, last->height())).y() > list->height() + 1)
        return fail("last option cannot scroll into view");
      keyClick(Qt::Key_Escape);
    } else if (phase == 23) {
      if (popup->property("visible").toBool() || list->property("count").toInt() != 0 ||
          choice->property("currentIndex").toInt() == 199)
        return fail("Escape should cancel and release the options");
      choice->setProperty("model", repeatPresets);
      workspace.setTextScale(1);
      choice->forceActiveFocus(Qt::TabFocusReason);
      keyClick(Qt::Key_Space);
      keyClick(Qt::Key_End);
      keyClick(Qt::Key_Return);
    } else {
      const auto *custom = timing->findChild<QObject *>("probeRepeatOptions");
      if (!custom || !custom->property("visible").toBool() ||
          popup->property("visible").toBool())
        return fail("selecting Custom should switch to the repeat parameter panel");
      return "DONE";
    }
  }
  ++phase;
  return {};
}
} // namespace daymark

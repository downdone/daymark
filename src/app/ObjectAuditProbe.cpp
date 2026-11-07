#include "ObjectAuditProbe.h"
#include "models/TaskWorkspace.h"
#include "models/WidgetViewModel.h"
#include "persistence/SqliteDatabase.h"
#include "platform/common/ProcessResources.h"
#include "services/TaskService.h"
#include <QFileInfo>
#include <QJsonDocument>
#include <QMap>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSGRendererInterface>
#include <QSaveFile>
#include <QScreen>
#include <QSet>
#include <QTimer>
namespace daymark {
namespace {

QString type(QObject *object) {
  auto name = QString::fromLatin1(object->metaObject()->className());
  name.remove(QRegularExpression("_QMLTYPE_[0-9]+|_QML_[0-9]+"));
  return name;
}
QObject *named(QQuickWindow *root, const char *name) {
  return root->findChild<QObject *>(QString::fromLatin1(name));
}
QWindow *owner(QObject *object) {
  for (auto *parent = object; parent; parent = parent->parent()) {
    if (auto *window = qobject_cast<QWindow *>(parent))
      return window;
    if (auto *item = qobject_cast<QQuickItem *>(parent); item && item->window())
      return item->window();
  }
  return nullptr;
}
QJsonObject histogram(const QMap<QString, int> &counts) {
  QJsonObject result;
  for (auto it = counts.begin(); it != counts.end(); ++it)
    result[it.key()] = it.value();
  return result;
}
struct Counts {
  int objects = 0, items = 0, visibleItems = 0, glyphs = 0, layers = 0,
      enabledLayers = 0, visibleLayers = 0, shapes = 0, loaders = 0;
  QMap<QString, int> classes;
  QJsonObject json() const {
    return {{"objects", objects},
            {"quickItems", items},
            {"visibilityCandidates", visibleItems},
            {"glyphs", glyphs},
            {"layerObjects", layers},
            {"enabledLayers", enabledLayers},
            {"visibleLayerCandidates", visibleLayers},
            {"shapes", shapes},
            {"loaders", loaders}};
  }
};
} // namespace
bool ObjectAuditProbe::seedFixture(const QString &path) {
  SqliteDatabase database;
  if (!database.open(path).ok())
    return false;
  TaskRepository repository(database.connection());
  SystemClock clock;
  TaskService service(repository, clock);
  for (int i = 0; i < 20; ++i)
    if (!service
             .create(QStringLiteral("object audit fixture %1").arg(i),
                     QStringLiteral("today"))
             .ok())
      return false;
  return true;
}
ObjectAuditProbe::ObjectAuditProbe(QQmlApplicationEngine &e, TaskWorkspace &w,
                                   WidgetViewModel &c, QQuickWindow *window,
                                   QString report)
    : engine(e), workspace(w), controller(c), mainWindow(window),
      destination(std::move(report)) {
  runClock.start();
}
void ObjectAuditProbe::capture(const QString &stage) {
  const auto before = readProcessResources();
  QList<QObject *> queue = engine.rootObjects();
  queue.append(&engine);
  queue.append(&workspace);
  queue.append(&controller);
  for (auto *window : QGuiApplication::allWindows())
    queue.append(window);
  QSet<QObject *> seen;
  Counts total;
  QMap<QWindow *, Counts> groups;
  QJsonObject componentSubtrees;
  QMap<QString, int> sourceSamples;
  const QSet<QString> inspect = {
      "CalendarPicker",    "DateField",      "TaskTimingFields",
      "TaskDetailsFields", "CalendarPanel",  "TaskRow",
      "WidgetTaskRow",     "SettingsDialog", "AccountWindow"};
  while (!queue.isEmpty()) {
    auto *object = queue.takeLast();
    if (!object || seen.contains(object))
      continue;
    seen.insert(object);
    queue.append(object->children());
    auto *item = qobject_cast<QQuickItem *>(object);
    if (item)
      for (auto *child : item->childItems())
        queue.append(child);
    if (auto *window = qobject_cast<QQuickWindow *>(object))
      queue.append(window->contentItem());
    const auto name = type(object);
    bool isGlyph = false;
    for (auto *meta = object->metaObject(); meta; meta = meta->superClass())
      if (QString::fromLatin1(meta->className()).startsWith("Glyph_QMLTYPE_") ||
          QByteArray(meta->className()) == "Glyph") {
        isGlyph = true;
        break;
      }
    auto *window = owner(object);
    auto &group = groups[window];
    if (name == "QQuickShaderEffectSource")
      ++sourceSamples[QString::number(object->property("samples").toInt())];
    if (inspect.contains(name)) {
      QSet<QObject *> nested;
      QList<QObject *> pending{object};
      while (!pending.isEmpty()) {
        auto *next = pending.takeLast();
        if (!next || nested.contains(next))
          continue;
        nested.insert(next);
        pending.append(next->children());
        if (auto *visual = qobject_cast<QQuickItem *>(next))
          for (auto *child : visual->childItems())
            pending.append(child);
      }
      auto summary = componentSubtrees.value(name).toObject();
      summary["instances"] = summary.value("instances").toInt() + 1;
      summary["sumReachableObjects"] =
          summary.value("sumReachableObjects").toInt() + nested.size();
      summary["maxReachableObjects"] = qMax(
          summary.value("maxReachableObjects").toInt(), int(nested.size()));
      componentSubtrees[name] = summary;
    }
    auto add = [&](Counts &value) {
      ++value.objects;
      ++value.classes[name];
      if (item) {
        ++value.items;
        if (item->isVisible() && window && window->isVisible())
          ++value.visibleItems;
      }
      if (isGlyph)
        ++value.glyphs;
      if (name == "QQuickItemLayer") {
        ++value.layers;
        if (object->property("enabled").toBool()) {
          ++value.enabledLayers;
          auto *parent = qobject_cast<QQuickItem *>(object->parent());
          if (parent && parent->isVisible() && window && window->isVisible())
            ++value.visibleLayers;
        }
      }
      if (name == "QQuickShape")
        ++value.shapes;
      if (name == "QQuickLoader")
        ++value.loaders;
    };
    add(group);
    add(total);
  }
  QJsonArray windows;
  const QStringList allowed = {
      "mainWindow",     "widgetWindow",  "desktopCalendarWindow",
      "settingsDialog", "accountWindow", "reminderPopup"};
  for (auto it = groups.begin(); it != groups.end(); ++it) {
    auto value = it.value().json();
    auto *window = it.key();
    value["owner"] = !window ? QStringLiteral("nonwindow")
                     : allowed.contains(window->objectName())
                         ? window->objectName()
                         : type(window);
    if (window) {
      value["visible"] = window->isVisible();
      value["exposed"] = window->isExposed();
      value["width"] = window->width();
      value["height"] = window->height();
      value["dpr"] = window->devicePixelRatio();
      if (auto *quick = qobject_cast<QQuickWindow *>(window)) {
        value["sceneGraphInitialized"] = quick->isSceneGraphInitialized();
        value["persistentSceneGraph"] = quick->isPersistentSceneGraph();
        value["persistentGraphics"] = quick->isPersistentGraphics();
        value["graphicsApi"] = int(quick->rendererInterface()->graphicsApi());
      }
    }
    windows.append(value);
  }
  auto result = total.json();
  result["stage"] = stage;
  result["monotonicMs"] = runClock.elapsed();
  result["memory"] = before;
  result["windows"] = windows;
  result["classes"] = histogram(total.classes);
  result["componentSubtrees"] = componentSubtrees;
  result["effectSourceSamples"] = histogram(sourceSamples);
  result["layerObjectCoverage"] = false;
  result["layerCounterNote"] =
      "Private ItemLayer instances are not all QObject children; zero "
      "layerObjects is NOT zero layers. Existing ShaderEffectSource "
      "instances/samples are reported without constructing layer groups.";
  result["taskValueSizeBytes"] = int(sizeof(Task));
  result["models"] =
      QJsonObject{{"mainWindow", workspace.tasks()->rowCount()},
                  {"today", workspace.todayTasks()->rowCount()},
                  {"deadline", workspace.deadlineTasks()->rowCount()},
                  {"widget", workspace.widgetTasks()->rowCount()}};
  records.append(result);
}
QString ObjectAuditProbe::advance() {
  if (!workspace.ready() || workspace.busy())
    return {};
  auto fail = [&](const char *key) {
    return QStringLiteral("MEM03 phase %1: %2")
        .arg(phase)
        .arg(QString::fromLatin1(key));
  };
  if (phase == 0) {
    workspace.setView("today");
    controller.setVisible(true);
    controller.selectForm(0);
    if (auto *screen = QGuiApplication::primaryScreen()) {
      mainWindow->setScreen(screen);
      mainWindow->setPosition(screen->availableGeometry().topLeft() +
                              QPoint(80, 80));
    }
    mainWindow->hide();
    mainWindow->showNormal();
    mainWindow->raise();
    phase = 1;
    settle.start();
    return {};
  }
  if (phase == 1) {
    if (workspace.todayTasks()->rowCount() != 20 ||
        workspace.widgetTasks()->rowCount() != 20)
      return fail("seed_projection");
    if (settle.elapsed() < 10000)
      return {};
    phase = 2;
  }
  if (settle.elapsed() < 700)
    return {};
  auto *settings = named(mainWindow, "settingsDialog");
  auto *account = named(mainWindow, "accountWindow");
  if (!settings || !account)
    return fail("surface_missing");
  if (phase == 2) {
    if (QGuiApplication::platformName() == QStringLiteral("windows")) {
      auto *widget =
          qobject_cast<QQuickWindow *>(named(mainWindow, "widgetWindow"));
      if (!mainWindow->isExposed() || !mainWindow->isSceneGraphInitialized() ||
          !widget || !widget->isExposed() || !widget->isSceneGraphInitialized())
        return fail("startup_windows_not_rendered");
    }
    capture("startup_twenty_tasks");
    if (!QMetaObject::invokeMethod(settings, "open"))
      return fail("settings_open");
  } else if (phase == 3) {
    capture("settings_open");
    if (!QMetaObject::invokeMethod(settings, "close"))
      return fail("settings_close");
  } else if (phase == 4) {
    capture("settings_closed");
    if (!QMetaObject::invokeMethod(account, "open"))
      return fail("account_open");
  } else if (phase == 5) {
    capture("account_open");
    if (!QMetaObject::invokeMethod(account, "close"))
      return fail("account_close");
  } else if (phase == 6) {
    capture("account_closed");
    if (!workspace.setPreference("desktopCalendarVisible", true))
      return fail("calendar_open");
  } else if (phase == 7) {
    capture("calendar_open");
    if (!workspace.setPreference("desktopCalendarVisible", false))
      return fail("calendar_close");
  } else if (phase == 8) {
    capture("calendar_closed");
    cycle = 0;
    QMetaObject::invokeMethod(settings, "open");
  } else if (phase == 9) {
    capture(QStringLiteral("repeat_open_%1").arg(cycle));
    QMetaObject::invokeMethod(settings, "close");
  } else if (phase == 10) {
    capture(QStringLiteral("repeat_closed_%1").arg(cycle));
    if (++cycle < 10) {
      QMetaObject::invokeMethod(settings, "open");
      phase = 9;
      settle.restart();
      return {};
    }
    mainWindow->hide();
  } else if (phase == 11) {
    capture("main_hidden_widget_only");
    mainWindow->show();
  } else if (phase == 12) {
    capture("main_restored");
    controller.setFormEnabled(2, true);
    controller.selectForm(2);
  } else if (phase == 13) {
    capture("ball");
    controller.selectForm(0);
  } else if (phase == 14) {
    capture("panel_returned");
    engine.collectGarbage();
  } else if (phase == 15) {
    capture("after_single_diagnostic_gc");
    mainWindow->hide();
    controller.selectForm(2);
  } else if (phase == 16) {
    capture("main_hidden_ball_only");
    controller.setVisible(false);
  } else if (phase == 17) {
    capture("all_windows_hidden");
    QJsonObject report{
        {"format", 1},
        {"passed", true},
        {"qt", QString::fromLatin1(qVersion())},
        {"applicationVersion", QCoreApplication::applicationVersion()},
        {"platform", QGuiApplication::platformName()},
        {"taskCount", 20},
        {"userDataAccessed", false},
        {"optimizationApplied", true},
        {"coverage",
         "Reachable QObjects via engine, application windows, workspace and "
         "controller, including visual children; excludes nonparented C++ "
         "value members, JS heap and native/GPU allocations"},
        {"observer", "Diagnostic objects and retained JSON are included; "
                     "class/property reads do not instantiate layer groups; "
                     "visibility candidates may be clipped/transparent"},
        {"snapshots", records}};
    if (QFileInfo::exists(destination))
      return fail("report_exists");
    QSaveFile output(destination);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly))
      return fail("report_open");
    const auto bytes = QJsonDocument(report).toJson();
    if (output.write(bytes) != bytes.size() || !output.commit())
      return fail("report_write");
    return "DONE";
  }
  ++phase;
  settle.restart();
  return {};
}
} // namespace daymark

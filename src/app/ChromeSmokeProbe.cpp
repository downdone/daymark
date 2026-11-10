#include "ChromeSmokeProbe.h"
#include "models/TaskWorkspace.h"
#include "platform/common/WindowChrome.h"
#include <QCoreApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlContext>
#include <QSet>
namespace daymark {
ChromeSmokeProbe::ChromeSmokeProbe(TaskWorkspace &w, WindowChromeController &c,
                                   QQuickWindow *v, QString path, bool layoutOnly)
    : workspace(w), controller(c), window(v), screenshot(std::move(path)), phase(layoutOnly ? 18 : 0) {}
QObject *ChromeSmokeProbe::find(QObject *root, const QString &name) const {
    QList<QObject *> pending{root};
    QSet<QObject *> seen;
    while (!pending.isEmpty()) {
        auto *object = pending.takeLast();
        if (!object || seen.contains(object))
            continue;
        seen.insert(object);
        if (object->objectName() == name)
            return object;
        pending.append(object->children());
        if (auto *item = qobject_cast<QQuickItem *>(object))
            for (auto *child : item->childItems())
                pending.append(child);
        if (auto *surface = qobject_cast<QQuickWindow *>(object))
            pending.append(surface->contentItem());
    }
    return nullptr;
}
bool ChromeSmokeProbe::pressKey(QQuickItem *item, int key) {
    if (!item || !item->window())
        return false;
    QPointer<QQuickWindow> target = item->window();
    target->requestActivate();
    item->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent down(QEvent::KeyPress, key, Qt::NoModifier),
        up(QEvent::KeyRelease, key, Qt::NoModifier);
    QCoreApplication::sendEvent(target, &down);
    // Enter can close a lazy control on key-down. Deliver release to the
    // surviving window, without touching the destroyed/detached button.
    if (target) QCoreApplication::sendEvent(target, &up);
    return true;
}
bool ChromeSmokeProbe::capture(QQuickWindow *target, const QString &suffix) {
    if (!target || !target->isVisible())
        return false;
    const auto image = target->grabWindow();
    return !image.isNull() &&
           (screenshot.isEmpty() || image.save(screenshot + suffix));
}
QString ChromeSmokeProbe::advance() {
    if (!workspace.ready() || workspace.busy())
        return {};
    auto fail = [&](const QString &message) {
        return QStringLiteral("CHROME01 phase %1: %2")
            .arg(phase)
            .arg(message);
    };
    auto *settings =
        qobject_cast<QQuickWindow *>(find(window, "settingsDialog"));
    auto *account = qobject_cast<QQuickWindow *>(find(window, "accountWindow"));
    auto allModes = [&](bool custom) {
        for (const auto *name :
             {"mainChrome", "settingsChrome", "accountChrome"}) {
            auto *binding = qobject_cast<WindowChrome *>(
                find(window, QString::fromLatin1(name)));
            if (!binding || binding->customEnabled() != custom)
                return false;
        }
        return true;
    };
    auto retained = [&] {
        return title &&
               title->property("text") == "Synthetic unsaved caption draft" &&
               window->property("draftDirty").toBool() &&
               window->property("editId") == taskId;
    };
    if (!settings || !account)
        return fail("tool window roots missing");
    if (phase >= 18) {
        const auto result = advanceLayout(settings, account);
        if (!result.isEmpty()) return result;
        ++phase;
        return {};
    }
    if (phase == 0) {
        if (!controller.supported() || !allModes(true) || !window->isExposed())
            return fail("native Windows chrome not active");
        auto *binding = qobject_cast<WindowChrome *>(find(window, "mainChrome"));
        if (!binding || binding->interactiveItems().size() != 3) return fail("header interactive exclusions not registered");
        workspace.setView("inbox");
        if (!workspace.createTask("Synthetic caption task"))
            return fail("seed task");
    } else if (phase == 1) {
        taskId = workspace.tasks()->taskIdAt(0);
        if (taskId.isEmpty())
            return fail("seed identity");
        QMetaObject::invokeMethod(window, "openEditor",
                                  Q_ARG(QVariant, QVariant(taskId)));
        title = qobject_cast<QQuickItem *>(find(window, "editTaskTitle"));
        if (!title)
            return fail("editor input");
        title->setProperty("text", "Synthetic unsaved caption draft");
        QMetaObject::invokeMethod(title, "textEdited");
        QMetaObject::invokeMethod(settings, "open");
    } else if (phase == 2) {
        auto *picker = find(settings, "titleBarModePicker");
        if (!picker || !retained() || !capture(settings, ".settings-light.png"))
            return fail("settings entry or initial draft");
        QMetaObject::invokeMethod(picker, "activateForVerification",
                                  Q_ARG(QVariant, QVariant(1)));
    } else if (phase == 3) {
        if (!allModes(false) || !retained() || !settings->isVisible())
            return fail("native switch retains draft and windows");
        auto *picker = find(settings, "titleBarModePicker");
        if (!picker || picker->property("currentIndex").toInt() != 1)
            return fail("external mode selection");
        QMetaObject::invokeMethod(picker, "activateForVerification",
                                  Q_ARG(QVariant, QVariant(0)));
    } else if (phase == 4) {
        if (!allModes(true) || !retained())
            return fail("Daymark switch retains draft");
        QMetaObject::invokeMethod(settings, "close");
        title->setProperty("text", "Synthetic caption task");
        window->setProperty("draftDirty", false);
        QMetaObject::invokeMethod(find(window, "taskEditor"), "close");
    } else if (phase == 5) {
        auto *toolbar = qobject_cast<QQuickItem *>(find(window, "pageToolbar"));
        auto *caption = qobject_cast<QQuickItem *>(find(window, "mainCaption"));
        auto *header = qobject_cast<QQuickItem *>(find(window, "integratedHeader"));
        if (!toolbar || !caption || !header || toolbar->y() < 12 ||
            toolbar->mapToScene({toolbar->width(), 0}).x() >
                caption->mapToScene({0, 0}).x() ||
            toolbar->y() + toolbar->height() > header->height())
            return fail("integrated title and actions overlap or lose top inset");
        const auto *heading = qobject_cast<QQuickItem *>(find(window, "pageTitle"));
        const qreal axis = heading ? heading->mapToScene({0, heading->height()/2}).y() : -1000;
        for (const auto *name : {"sidebarWordmark", "toggleSidebar", "pageOptionsAction", "mainCaption"}) {
            const auto *item = qobject_cast<QQuickItem *>(find(window, QString::fromLatin1(name)));
            if (!item)
                return fail(QStringLiteral("missing aligned title-bar item %1").arg(QString::fromLatin1(name)));
            const auto itemAxis = item->mapToScene({0, item->height()/2}).y();
            if (qAbs(itemAxis-axis)>1.5)
                return fail(QStringLiteral("title-bar axis: %1 y=%2, heading y=%3")
                                .arg(QString::fromLatin1(name)).arg(itemAxis).arg(axis));
        }
        if (!capture(window, ".main-light.png"))
            return fail("main title bar capture");
        auto *close = qobject_cast<QQuickItem *>(find(caption, "captionClose"));
        const auto *feedback = qobject_cast<QQuickItem *>(find(close, "captionFeedback"));
        if (!close || !feedback || close->height() != caption->height() ||
            feedback->size() != QSizeF(36, 28) ||
            QLineF(close->mapToScene(close->boundingRect().center()),
                   feedback->mapToScene(feedback->boundingRect().center())).length() > 1)
            return fail("caption feedback must be compact and centered in the original hit area");
        const auto at = close->mapToScene(close->boundingRect().center());
        QMouseEvent hover(QEvent::MouseMove, at, window->mapToGlobal(at),
                          Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &hover);
        if (feedback->property("color").value<QColor>() != QColor("#c42b1c") ||
            !capture(window, ".close-hover.png"))
            return fail("compact close hover feedback");
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(window, &leave);
        // Calendar anchors are scene coordinates; the inspector is in the body
        // below the integrated header. Compare the actual arrow in scene space.
        const QPointF anchor(600, 200);
        if (!QMetaObject::invokeMethod(window, "prepareInspectorPosition",
                                      Q_ARG(QVariant, QVariant(taskId)),
                                      Q_ARG(QVariant, QVariant(anchor))))
            return fail("calendar inspector anchor boundary");
        auto *editor = qobject_cast<QQuickItem *>(find(window, "taskEditor"));
        auto *surface = find(window, "taskEditorSurface");
        if (!editor || !surface ||
            (editor->mapToScene(surface->property("sourcePoint").toPointF()) - anchor).manhattanLength() > 1)
            return fail("calendar arrow shifted by integrated header");
        QMetaObject::invokeMethod(account, "open");
    } else if (phase == 6) {
        auto *login = find(account, "syncLogin");
        auto *origin = find(account, "syncServerOrigin");
        if (!login || !origin)
            return fail("account inputs");
        account->setProperty("serviceMode", 1);
        login->setProperty("text", "synthetic-caption-user");
        origin->setProperty("text", "https://custom.example.invalid");
        if (!controller.requestMode(0) || !controller.requestMode(1))
            return fail("switch while account is visible");
        workspace.setThemeMode(2);
        workspace.setTextScale(2);
        account->resize(420, 440);
    } else if (phase == 7) {
        if (!allModes(true) || !account->isVisible() ||
            find(account, "syncLogin")->property("text") !=
                "synthetic-caption-user" ||
            find(account, "syncServerOrigin")->property("text") !=
                "https://custom.example.invalid")
            return fail("account draft changed");
        if (!capture(account, ".account-dark-200.png"))
            return fail("account dark large title");
        if (!pressKey(qobject_cast<QQuickItem *>(
                find(find(account, "accountCaption"), "captionClose"))))
            return fail("account close keyboard");
    } else if (phase == 8) {
        if (account->isVisible())
            return fail("account standard close route");
        workspace.setTextScale(1);
        workspace.setThemeMode(1);
        window->requestActivate();
    } else if (phase == 9) {
        auto *binding =
            qobject_cast<WindowChrome *>(find(window, "mainChrome"));
        binding->toggleMaximize();
    } else if (phase == 10) {
        if (window->windowState() != Qt::WindowMaximized ||
            !controller.requestMode(0))
            return fail("native mode while maximized");
    } else if (phase == 11) {
        if (window->windowState() != Qt::WindowMaximized) {
            qWarning() << "CHROME after native switch state/visibility/geometry" << window->windowState() << window->visibility() << window->geometry();
            return fail("maximized state lost after native switch event delivery");
        }
        if (!controller.requestMode(1)) {
            qWarning() << controller.errorText();
            return fail("Daymark mode native request failed");
        }
    } else if (phase == 12) {
        auto *minimize = qobject_cast<QQuickItem *>(
            find(find(window, "mainCaption"), "captionMinimize"));
        auto *maximize = qobject_cast<QQuickItem *>(
            find(find(window, "mainCaption"), "captionMaximize"));
        if (!pressKey(minimize, Qt::Key_Tab) || !maximize || !maximize->hasActiveFocus())
            return fail("caption Tab navigation order");
        if (!pressKey(qobject_cast<QQuickItem *>(
                find(find(window, "mainCaption"), "captionMaximize")), Qt::Key_Return))
            return fail("restore keyboard");
    } else if (phase == 13) {
        if (window->windowState() != Qt::WindowNoState)
            return fail("restore toggled more than once");
        if (!pressKey(qobject_cast<QQuickItem *>(
                find(find(window, "mainCaption"), "captionMinimize"))))
            return fail("minimize keyboard");
    } else if (phase == 14) {
        if (window->windowState() != Qt::WindowMinimized)
            return fail("minimize did not use normal OS state");
        window->showNormal();
    } else if (phase == 15)
        window->hide();
    else if (phase == 16) {
        if (!controller.requestMode(0) || window->isVisible() ||
            !controller.requestMode(1) || window->isVisible())
            return fail("hidden mode switch activated window");
        window->showNormal();
    } else if (phase == 17) {
        if (!allModes(true) || !capture(window, ".restored.png"))
            return fail("hidden resource restore");
        return "DONE";
    }
    ++phase;
    return {};
}

QString ChromeSmokeProbe::compareLayout(QObject *root, const QStringList &names, bool reset) {
    if (reset) layoutBaseline.clear();
    for (const auto &name : names) {
        const auto *item = qobject_cast<QQuickItem *>(find(root, name));
        if (!item || !item->isVisible()) return "CHROME02 layout item missing: " + name;
        const QRectF actual(item->mapToScene({0, 0}), item->size());
        if (reset) layoutBaseline.insert(name, actual);
        const auto expected = layoutBaseline.value(name);
        if (qAbs(actual.y() - expected.y()) > 1 ||
            (name.endsWith("Header") && qAbs(actual.height() - expected.height()) > 1))
            return QStringLiteral("CHROME02 phase %1: %2 moved: y %3 -> %4, height %5 -> %6")
                .arg(phase).arg(name).arg(expected.y()).arg(actual.y())
                .arg(expected.height()).arg(actual.height());
    }
    return {};
}

QString ChromeSmokeProbe::advanceLayout(QQuickWindow *settings, QQuickWindow *account) {
    auto fail = [this](const QString &text) {
        return QStringLiteral("CHROME02 phase %1: %2").arg(phase).arg(text);
    };
    if (phase == 18) {
        if (!controller.supported() || !window->isExposed()) return fail("native Windows surface missing");
        if (!workspace.saveCatalog({{"kind", "project"}, {"title", "Synthetic long category for stable title layout"}}))
            return fail("category seed failed");
    } else if (phase == 19) {
        for (const auto &entry : workspace.catalog()) {
            const auto value = entry.toMap();
            if (value.value("kind") == "project") categoryId = value.value("id").toString();
        }
        if (categoryId.isEmpty() || !workspace.saveCatalog({{"kind", "tag"}, {"title", "Synthetic layout tag"}}))
            return fail("tag seed failed");
    } else if (phase < 100) {
        const int sample = (phase - 20) / 2, group = sample / 10, page = sample % 10;
        const bool apply = (phase - 20) % 2 == 0;
        if (apply) {
            if (phase == 20) {
                for (const auto &entry : workspace.catalog()) {
                    const auto value = entry.toMap();
                    if (value.value("kind") == "tag") tagId = value.value("id").toString();
                }
                if (tagId.isEmpty()) return fail("tag identity missing");
            }
            if (page == 0) {
                workspace.setTextScale(group >= 2 ? 2 : 1);
                workspace.setThemeMode(group == 1 || group == 2 ? 2 : 1);
                if (!controller.requestMode(group % 2 ? 0 : 1)) return fail("mode setup failed");
                window->resize(group % 2 ? 880 : 1120, 720);
            }
            auto *search = find(window, "taskSearch");
            search->setProperty("text", "");
            const QStringList views{"inbox", "today", group % 2 ? "anytime" : "scheduled", "important", "project:" + categoryId,
                                    "tag:" + tagId, "completed", "cancelled", "trash", "inbox"};
            workspace.setView(views.at(page));
            if (page == 9) search->setProperty("text", "Synthetic");
        } else {
            const auto result = compareLayout(window, {"integratedHeader", "sidebarBrandHeader", "sidebarWordmark",
                "toggleSidebar", "navigationScroll", "pageToolbar", "taskContent", "taskHeaderViewport"}, page == 0);
            if (!result.isEmpty()) return result;
            const QStringList themes{"inbox", "today", group % 2 ? "all" : "scheduled", "important",
                                     "", "", "completed", "completed", "", ""};
            if (!themes.at(page).isEmpty()) {
                const auto *tile = find(window, "navigation-" + themes.at(page));
                const auto *title = find(window, "pageTitle");
                if (!tile || !title || title->property("color").value<QColor>() != tile->property("tint").value<QColor>())
                    return fail("page title color differs from its sidebar theme");
            }
            if (page == 1 && (group == 0 || group == 3) &&
                !capture(window, group == 0 ? ".stable-today-light.png" : ".stable-today-large.png"))
                return fail("stable today capture failed");
        }
    } else if (phase < 132) {
        const int sample = (phase - 100) / 2, group = sample / 4, category = sample % 4;
        if ((phase - 100) % 2 == 0) {
            if (category == 0) {
                find(window, "taskSearch")->setProperty("text", "");
                workspace.setTextScale(group >= 2 ? 2 : 1);
                workspace.setThemeMode(group % 2 ? 2 : 1);
                if (!controller.requestMode(group % 2 ? 0 : 1)) return fail("settings mode setup failed");
                settings->resize(group % 2 ? 640 : 860, 680);
                QMetaObject::invokeMethod(settings, "open");
            }
            QMetaObject::invokeMethod(settings, "activateVerificationCategory", Q_ARG(QVariant, QVariant(category)));
        } else {
            const auto result = compareLayout(settings, {"settingsIntegratedHeader", "settingsCategories", "settingsScroll"}, category == 0);
            if (!result.isEmpty()) return result;
            if (category == 1) {
                for (const auto *name : {"widgetFormSelection", "widgetCategoryRangeSetting"}) {
                    const auto *picker = find(settings, QString::fromLatin1(name));
                    const auto *background = picker ? picker->property("background").value<QObject *>() : nullptr;
                    if (!background || !background->property("quietOutline").toBool())
                        return fail("settings dropdown lost quiet field styling");
                }
                const auto *solid = find(settings, "widgetSolidBackgroundSetting");
                if (!solid || solid->property("highlightRow").toBool())
                    return fail("settings switch highlights the whole row");
                const auto *enabledPanel = find(settings, "widgetPanelEnabled");
                const auto *track = enabledPanel ? enabledPanel->property("indicator").value<QObject *>() : nullptr;
                const auto green = track ? track->property("color").value<QColor>() : QColor();
                if (!enabledPanel || !enabledPanel->property("checked").toBool() ||
                    !green.isValid() || green.green() <= qMax(green.red(),green.blue()))
                    return fail("enabled settings switch lost its green track");
                if ((group == 0 || group == 3) &&
                    !capture(settings, group == 0 ? ".settings-fields-light.png" : ".stable-settings-large.png"))
                    return fail("settings capture failed");
            }
        }
    } else if (phase == 132) {
        QMetaObject::invokeMethod(settings, "close");
        workspace.setTextScale(2);
        workspace.setThemeMode(2);
        if (!controller.requestMode(1)) return fail("restore custom controls failed");
        QMetaObject::invokeMethod(account, "open");
        account->resize(420, 440);
        account->setProperty("syncRegisterMode", false);
    } else if (phase == 133) {
        const auto result = compareLayout(account, {"accountIntegratedHeader", "accountWordmark", "accountScroll"}, true);
        if (!result.isEmpty()) return result;
        const auto *brand = qobject_cast<QQuickItem *>(find(account, "accountWordmark"));
        const auto *form = qobject_cast<QQuickItem *>(find(account, "accountContent"));
        if (!brand || !form || qAbs(brand->mapToScene({0,0}).x() - form->mapToScene({0,0}).x()) > 1)
            return fail("account brand and form axes differ");
        const auto *caption = qobject_cast<QQuickItem *>(find(account, "accountCaption"));
        if (!caption || qAbs(caption->mapToScene({0,caption->height()/2}).y()-brand->mapToScene({0,brand->height()/2}).y())>1.5)
            return fail("account window controls and brand are not aligned");
        account->setProperty("syncRegisterMode", true);
    } else if (phase == 134) {
        const auto result = compareLayout(account, {"accountIntegratedHeader", "accountWordmark", "accountScroll"}, false);
        if (!result.isEmpty()) return result;
        QMetaObject::invokeMethod(account, "close");
        workspace.setTextScale(1);
        workspace.setThemeMode(1);
        workspace.setView("inbox");
        window->resize(1120, 720);
    } else if (phase == 135) {
        auto *engine = qmlEngine(window);
        if (!engine) return fail("QML engine missing");
        QQmlComponent component(engine);
        component.setData("import QtQuick\nQtObject { property bool ready: true; property bool busy: false; "
                          "property string availableVersion: '9.9.9'; property int restartCount: 0; "
                          "signal updateReady(); signal changed(); function restart() { ++restartCount } }", {});
        updateStub = component.create(engine->rootContext());
        if (!updateStub) return fail(component.errorString());
        updateStub->setParent(window);
        window->setProperty("updateController", QVariant::fromValue(updateStub.data()));
        auto *input = qobject_cast<QQuickItem *>(find(window, "newTaskTitle"));
        input->forceActiveFocus();
        QMetaObject::invokeMethod(updateStub, "updateReady");
    } else if (phase == 136) {
        auto *notice = qobject_cast<QQuickItem *>(find(window, "updateReadyNotice"));
        auto *surface = find(window, "updateReadySurface");
        auto *body = find(window, "updateReadyMessage");
        auto *input = qobject_cast<QQuickItem *>(find(window, "newTaskTitle"));
        if (!notice || !surface || !body || !input->hasActiveFocus() ||
            surface->property("color") == window->color() || !body->property("text").toString().contains("9.9.9"))
            return fail("notice lacks distinct surface/version or stole input focus");
        const QRectF bounds(notice->mapToScene({0,0}), notice->size());
        const auto *header = qobject_cast<QQuickItem *>(find(window, "integratedHeader"));
        const auto *footer = qobject_cast<QQuickItem *>(find(window, "pageFooter"));
        const auto *entry = qobject_cast<QQuickItem *>(find(window, "newTaskTitle"));
        if (bounds.top() < header->height() || bounds.left() < window->property("sidebarWidth").toReal()+16 ||
            bounds.right() > window->width() || bounds.bottom() > footer->mapToScene({0,0}).y()-8 ||
            bounds.top() < entry->mapToScene({0,entry->height()}).y()+8)
            return fail("notice overlaps caption, navigation, quick entry or footer");
        if (!capture(window, ".update-ready-light.png")) return fail("notice capture failed");
        qobject_cast<QQuickItem *>(find(window, "taskList"))->forceActiveFocus();
        if (!pressKey(qobject_cast<QQuickItem *>(find(window, "deferUpdate")))) return fail("defer keyboard failed");
    } else if (phase == 137) {
        if (find(window, "updateReadyNotice") || updateStub->property("restartCount").toInt() != 0 ||
            !qobject_cast<QQuickItem *>(find(window, "taskList"))->hasActiveFocus())
            return fail("defer did not release notice/restore focus, or restarted");
        workspace.setTextScale(2);
        workspace.setThemeMode(2);
        if (!workspace.setPreference("followSystemAccessibility", false) || !workspace.setPreference("highContrast", true))
            return fail("high contrast preference setup failed");
        window->resize(880, 720);
        updateStub->setProperty("busy", true);
        QMetaObject::invokeMethod(updateStub, "updateReady");
    } else if (phase == 138) {
        auto *restart = qobject_cast<QQuickItem *>(find(window, "restartUpdate"));
        auto *notice = qobject_cast<QQuickItem *>(find(window, "updateReadyNotice"));
        if (!restart || restart->isEnabled() || !notice || notice->height() >= window->height() - 160)
            return fail("busy or large notice geometry incorrect");
        if (!capture(window, ".update-ready-dark-large.png")) return fail("dark large notice capture failed");
        updateStub->setProperty("busy", false);
    } else if (phase == 139) {
        auto *restart = qobject_cast<QQuickItem *>(find(window, "restartUpdate"));
        if (!restart || !restart->isEnabled() || !pressKey(restart, Qt::Key_Return))
            return fail("restart keyboard failed");
    } else if (phase == 140) {
        if (updateStub->property("restartCount").toInt() != 1 || find(window, "updateReadyNotice"))
            return fail("restart intent must be delivered exactly once and release notice");
        window->setProperty("updateController", QVariant::fromValue<QObject *>(nullptr));
        delete updateStub.data();
        qInfo() << "PASS: CHROME02 40 main views, 16 settings categories, account axes and notification UI (synthetic controller)";
        return "DONE";
    }
    return {};
}
} // namespace daymark

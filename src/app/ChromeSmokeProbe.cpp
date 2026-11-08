#include "ChromeSmokeProbe.h"
#include "models/TaskWorkspace.h"
#include "platform/common/WindowChrome.h"
#include <QCoreApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
namespace daymark {
ChromeSmokeProbe::ChromeSmokeProbe(TaskWorkspace &w, WindowChromeController &c,
                                   QQuickWindow *v, QString path)
    : workspace(w), controller(c), window(v), screenshot(std::move(path)) {}
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
    item->window()->requestActivate();
    item->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent down(QEvent::KeyPress, key, Qt::NoModifier),
        up(QEvent::KeyRelease, key, Qt::NoModifier);
    QCoreApplication::sendEvent(item->window(), &down);
    QCoreApplication::sendEvent(item->window(), &up);
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
    auto fail = [&](const char *message) {
        return QStringLiteral("CHROME01 phase %1: %2")
            .arg(phase)
            .arg(QString::fromLatin1(message));
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
        if (!capture(window, ".main-light.png"))
            return fail("main title bar capture");
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
} // namespace daymark

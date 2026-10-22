#pragma once

#include <QQmlApplicationEngine>
#include "models/WidgetViewModel.h"
#include "models/TaskWorkspace.h"
#include "platform/common/QtWindowPlatform.h"
#include "platform/common/SingleInstance.h"
#include "platform/common/DesktopTray.h"
#include "platform/common/WindowStateStore.h"
#include <QTemporaryDir>

namespace daymark {

// Composition root. Business services will be injected here as slices are delivered.
class AppRuntime final {
public:
    bool start(bool smokeTest, const QString& dataDirectory = {}, const QString& screenshotPath = {});
    int startupExitCode() const { return m_startupExitCode; }

private:
    std::unique_ptr<QTemporaryDir> m_temporaryData;
    SingleInstance m_instance;
    QtWindowPlatform m_windowPlatform;
    WidgetViewModel m_widget{m_windowPlatform};
    TaskWorkspace m_workspace;
    QQmlApplicationEngine m_engine;
    std::unique_ptr<DesktopTray> m_tray;
    std::unique_ptr<WindowStateStore> m_mainState;
    bool m_qmlWarnings = false;
    int m_startupExitCode = 1;
};

} // namespace daymark

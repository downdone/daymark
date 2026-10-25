#pragma once

#include <QQmlApplicationEngine>
#include "models/WidgetViewModel.h"
#include "models/TaskWorkspace.h"
#include "platform/common/QtWindowPlatform.h"
#include "platform/common/SingleInstance.h"
#include "platform/common/DesktopTray.h"
#include "platform/common/WindowStateStore.h"
#include "platform/common/StartupController.h"
#include "platform/common/SystemEventFilter.h"
#include <QTemporaryDir>
#include "platform/common/ApplicationMenus.h"

namespace daymark {

// Composition root. Business services will be injected here as slices are delivered.
class AppRuntime final {
public:
    bool start(bool smokeTest, const QString& dataDirectory = {}, const QString& screenshotPath = {},int soakSeconds=0,const QString& soakReport={},const QString& smokeProfile={});
    int verificationPhase() const {return m_verificationPhase;}
    bool verificationCompleted() const {return m_verificationCompleted;}
    int startupExitCode() const { return m_startupExitCode; }

private:
    std::unique_ptr<QTemporaryDir> m_temporaryData;
    SingleInstance m_instance;
    QtWindowPlatform m_windowPlatform;
    WidgetViewModel m_widget{m_windowPlatform};
    TaskWorkspace m_workspace;
    std::unique_ptr<StartupController> m_startup;
    QQmlApplicationEngine m_engine;
    std::unique_ptr<DesktopTray> m_tray;
    std::unique_ptr<ApplicationMenus> m_menus;
    std::unique_ptr<WindowStateStore> m_mainState;
    std::unique_ptr<WindowStateStore> m_calendarState;
    std::unique_ptr<SystemEventFilter> m_systemEvents;
    bool m_qmlWarnings = false;
    bool m_verificationCompleted=false;
    int m_verificationPhase=0;
    int m_startupExitCode = 1;
};

} // namespace daymark

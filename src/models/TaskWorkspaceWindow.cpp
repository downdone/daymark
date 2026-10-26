// Main-thread window/settings adapter. Native layer policy stays in platform/.
#include "TaskWorkspace.h"
#include "platform/common/QtWindowPlatform.h"
#include <QWindow>

namespace daymark {
bool TaskWorkspace::attachDesktopCalendarWindow(QWindow* window)
{
    if (!window) {
        m_error = {ErrorCode::PermissionError, QStringLiteral("windowLayer")};
        emit stateChanged();
        return false;
    }
    if (!m_calendarPlatform) {
        m_calendarPlatform = std::make_unique<QtWindowPlatform>();
        connect(m_calendarPlatform.get(), &QtWindowPlatform::layerRestoreFailed, this, [this](QWindow* target) {
            if (target == m_calendarWindow) {
                m_error = {ErrorCode::PermissionError, QStringLiteral("windowLayer")};
                emit stateChanged();
            }
        });
    }
    m_calendarWindow = window;
    return applyCalendarLayer(m_preferences.value(QStringLiteral("desktopCalendarPinned")).toBool(),
                              m_preferences.value(QStringLiteral("desktopCalendarTopmost")).toBool());
}
bool TaskWorkspace::applyCalendarLayer(bool desktop, bool topmost)
{
    if (!m_calendarWindow || !m_calendarPlatform)
        return true;
    const auto result = desktop ? m_calendarPlatform->setDesktopPinned(m_calendarWindow, true)
                                : m_calendarPlatform->setAlwaysOnTop(m_calendarWindow, topmost);
    if (result.succeeded())
        return true;
    m_error = {ErrorCode::PermissionError, QStringLiteral("windowLayer")};
    emit stateChanged();
    return false;
}
}

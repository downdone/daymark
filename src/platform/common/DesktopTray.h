#pragma once
#include "models/WidgetViewModel.h"
#include <QMenu>
#include <QPointer>
#include <QSystemTrayIcon>

namespace daymark {
class DesktopTray final : public QObject {
    Q_OBJECT
public:
    DesktopTray(WidgetViewModel& widget,QWindow* mainWindow,QObject* parent=nullptr);
    bool notify(const QString& title,const QString& message);
private:
    WidgetViewModel& m_widget;
    QPointer<QWindow> m_mainWindow;
    QMenu m_menu;
    QSystemTrayIcon m_tray;
};
}

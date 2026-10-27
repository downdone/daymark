#include "DesktopTray.h"
#include <QAction>
#include <QGuiApplication>
#include <QMetaObject>

namespace daymark {
bool DesktopTray::notify(const QString& title,const QString& message)
{
    if(!m_tray.isVisible() || !QSystemTrayIcon::supportsMessages())return false;
    m_tray.showMessage(title,message,QSystemTrayIcon::Information,10000);
    return true; // Submitted to OS; delivery is not observable through QSystemTrayIcon.
}
DesktopTray::DesktopTray(WidgetViewModel& widget,QWindow* mainWindow,QObject* parent)
    :QObject(parent),m_widget(widget),m_mainWindow(mainWindow)
{
    m_menu.setObjectName(QStringLiteral("trayContextMenu"));
    const auto showMain=[this]{if(m_mainWindow){if(m_mainWindow->windowState()==Qt::WindowMinimized)m_mainWindow->showNormal();else m_mainWindow->setVisible(true);m_mainWindow->requestActivate();}};
    auto name=[](QAction* action,const char* id){action->setObjectName(QString::fromLatin1(id));return action;};
    name(m_menu.addAction(tr("打开主窗口"),this,showMain),"trayShowMain");
    name(m_menu.addAction(tr("新建任务…"),this,[this,showMain]{showMain();if(m_mainWindow)QMetaObject::invokeMethod(m_mainWindow,"newTask",Q_ARG(QVariant,QVariant(QStringLiteral("inbox"))));}),"trayNewTask");
    m_menu.addSeparator();
    auto* visibility=name(m_menu.addAction(tr("显示任务组件"),this,[this]{m_widget.setVisible(!m_widget.visible());}),"trayToggleWidget");
    m_menu.addSeparator();
    auto* pass=name(m_menu.addAction(tr("鼠标穿透")),"trayClickThrough");pass->setCheckable(true);
    connect(pass,&QAction::triggered,this,[this](bool checked){m_widget.setClickThrough(checked);});
    auto* restore=name(m_menu.addAction(tr("恢复组件点击"),this,[this]{m_widget.setClickThrough(false);}),"trayRestoreInput");
    name(m_menu.addAction(tr("重置组件位置"),this,[this]{m_widget.resetPosition();m_widget.setVisible(true);}),"trayResetWidget");
    m_menu.addSeparator();
    name(m_menu.addAction(tr("设置…"),this,[this,showMain]{showMain();if(m_mainWindow)QMetaObject::invokeMethod(m_mainWindow,"openTool",Q_ARG(QVariant,QVariant(QStringLiteral("settings"))));}),"traySettings");
    m_menu.addSeparator();
    name(m_menu.addAction(tr("退出 Daymark"),this,[this]{
        if(m_mainWindow){m_mainWindow->setProperty("forceQuit",true);m_mainWindow->close();}
    }),"trayQuit");
    const auto refresh=[this,pass,restore,visibility]{pass->setChecked(m_widget.clickThrough());pass->setEnabled(m_widget.trayAvailable());restore->setEnabled(m_widget.clickThrough());visibility->setText(m_widget.visible()?tr("隐藏任务组件"):tr("显示任务组件"));};
    connect(&m_widget,&WidgetViewModel::stateChanged,this,refresh);
    connect(&m_menu,&QMenu::aboutToShow,this,refresh);refresh();
    // Build commands even without a shell tray; never claim recovery is installed.
    if(!QSystemTrayIcon::isSystemTrayAvailable()){m_widget.setTrayAvailable(false);return;}
    m_tray.setIcon(QGuiApplication::windowIcon());m_tray.setToolTip(QStringLiteral("Daymark"));
    connect(&m_tray,&QSystemTrayIcon::activated,this,[showMain](QSystemTrayIcon::ActivationReason reason){
        if(reason==QSystemTrayIcon::Trigger || reason==QSystemTrayIcon::DoubleClick)showMain();
    });
    m_tray.setContextMenu(&m_menu);m_tray.show();
    m_widget.setTrayAvailable(true); // Recovery actions exist before enabling input transparency.
}
}

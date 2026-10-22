#include "DesktopTray.h"
#include <QAction>
#include <QPainter>
#include <QPixmap>

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
    if(!QSystemTrayIcon::isSystemTrayAvailable()){m_widget.setTrayAvailable(false);return;}
    QPixmap pixmap(32,32);pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(QStringLiteral("#315DB5")));painter.setPen(Qt::NoPen);painter.drawRoundedRect(2,2,28,28,7,7);
    painter.setPen(QPen(Qt::white,3,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.drawLine(9,17,14,22);painter.drawLine(14,22,24,10);painter.end();
    m_tray.setIcon(QIcon(pixmap));m_tray.setToolTip(QStringLiteral("Daymark"));
    const auto showMain=[this]{if(m_mainWindow){m_mainWindow->showNormal();m_mainWindow->requestActivate();}};
    m_menu.addAction(tr("打开主窗口"),this,showMain);
    m_menu.addAction(tr("显示/隐藏桌面组件"),this,[this]{m_widget.setVisible(!m_widget.visible());});
    auto* pass=m_menu.addAction(tr("鼠标穿透"));pass->setCheckable(true);
    connect(pass,&QAction::triggered,this,[this](bool checked){m_widget.setClickThrough(checked);});
    m_menu.addAction(tr("关闭鼠标穿透（恢复点击）"),this,[this]{m_widget.setClickThrough(false);});
    m_menu.addAction(tr("重置组件位置"),this,[this]{m_widget.resetPosition();m_widget.setVisible(true);});
    m_menu.addSeparator();
    m_menu.addAction(tr("退出 Daymark"),this,[this]{
        if(m_mainWindow){m_mainWindow->setProperty("forceQuit",true);m_mainWindow->close();}
    });
    connect(&m_widget,&WidgetViewModel::stateChanged,this,[this,pass]{pass->setChecked(m_widget.clickThrough());});
    connect(&m_tray,&QSystemTrayIcon::activated,this,[showMain](QSystemTrayIcon::ActivationReason reason){
        if(reason==QSystemTrayIcon::Trigger || reason==QSystemTrayIcon::DoubleClick)showMain();
    });
    m_tray.setContextMenu(&m_menu);m_tray.show();
    m_widget.setTrayAvailable(true); // Recovery actions exist before enabling input transparency.
}
}

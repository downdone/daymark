#include "WindowStateStore.h"
#include <QGuiApplication>
#include <QScreen>
namespace daymark {
WindowStateStore::WindowStateStore(QWindow* window,const QString& path,QObject* parent):QObject(parent),m_window(window),m_settings(path,QSettings::IniFormat)
{
    if(!window)return;
    const auto geometry=m_settings.value(QStringLiteral("main/geometry"),window->geometry()).toRect();
    window->setGeometry(QRect(geometry.topLeft(),geometry.size().expandedTo(QSize(880,580))));normalize();m_normal=window->geometry();
    if(m_settings.value(QStringLiteral("main/maximized"),false).toBool())window->showMaximized();
    m_timer.setSingleShot(true);m_timer.setInterval(500);connect(&m_timer,&QTimer::timeout,this,[this]{save();});
    auto changed=[this]{if(m_window && m_window->windowState()==Qt::WindowNoState)m_normal=m_window->geometry();m_timer.start();};
    connect(window,&QWindow::xChanged,this,changed);connect(window,&QWindow::yChanged,this,changed);
    connect(window,&QWindow::widthChanged,this,changed);connect(window,&QWindow::heightChanged,this,changed);connect(window,&QWindow::windowStateChanged,this,changed);
    connect(qGuiApp,&QGuiApplication::screenRemoved,this,[this]{normalize();});
    connect(qGuiApp,&QCoreApplication::aboutToQuit,this,[this]{save();});
}
void WindowStateStore::normalize()
{
    if(!m_window)return;
    auto* screen=QGuiApplication::screenAt(m_window->geometry().center());if(!screen)screen=QGuiApplication::primaryScreen();if(!screen)return;
    const auto available=screen->availableGeometry();const auto size=m_window->size().boundedTo(available.size());
    const QPoint position(qBound(available.left(),m_window->x(),available.right()-size.width()+1),qBound(available.top(),m_window->y(),available.bottom()-size.height()+1));
    m_window->setGeometry(QRect(position,size));
}
void WindowStateStore::save()
{
    if(!m_window)return;
    m_settings.setValue(QStringLiteral("main/geometry"),m_normal);
    if(m_window->windowState()!=Qt::WindowMinimized)m_settings.setValue(QStringLiteral("main/maximized"),m_window->windowState()==Qt::WindowMaximized);
    m_settings.sync();
    if(m_settings.status()!=QSettings::NoError)emit saveFailed();
}
}

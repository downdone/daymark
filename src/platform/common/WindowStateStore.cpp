#include "WindowStateStore.h"
#include <QGuiApplication>
#include <QScreen>
namespace daymark {
WindowStateStore::WindowStateStore(QWindow* window,const QString& path,QObject* parent):QObject(parent),m_window(window),m_settings(path,QSettings::IniFormat)
{
    if(!window)return;
    m_timer.setSingleShot(true);m_timer.setInterval(500);connect(&m_timer,&QTimer::timeout,this,[this]{save();});
    const auto geometry=m_settings.value(QStringLiteral("main/geometry"),window->geometry()).toRect();
    window->setGeometry(QRect(geometry.topLeft(),geometry.size().expandedTo(QSize(880,580))));normalize();m_normal=window->geometry();
    if(m_settings.value(QStringLiteral("main/maximized"),false).toBool())window->showMaximized();
    auto changed=[this]{if(m_window && m_window->windowState()==Qt::WindowNoState)m_normal=m_window->geometry();m_timer.start();};
    connect(window,&QWindow::xChanged,this,changed);connect(window,&QWindow::yChanged,this,changed);
    connect(window,&QWindow::widthChanged,this,changed);connect(window,&QWindow::heightChanged,this,changed);connect(window,&QWindow::windowStateChanged,this,changed);
    connect(qGuiApp,&QGuiApplication::screenRemoved,this,[this]{normalize();});
    auto watch=[this](QScreen* screen){connect(screen,&QScreen::availableGeometryChanged,this,[this]{normalize();});};
    connect(qGuiApp,&QGuiApplication::screenAdded,this,[this,watch](QScreen* screen){watch(screen);normalize();});
    for(auto* screen:QGuiApplication::screens())watch(screen);
    connect(qGuiApp,&QCoreApplication::aboutToQuit,this,[this]{save();});
}
void WindowStateStore::normalize()
{
    if(!m_window)return;
    const auto normal=m_window->windowState()==Qt::WindowNoState || !m_normal.isValid() ? m_window->geometry() : m_normal;
    auto* screen=QGuiApplication::screenAt(normal.center());if(!screen)screen=QGuiApplication::primaryScreen();if(!screen)return;
    const auto available=screen->availableGeometry();const auto size=normal.size().expandedTo(QSize(880,580)).boundedTo(available.size());
    const QPoint position(qBound(available.left(),normal.x(),available.right()-size.width()+1),qBound(available.top(),normal.y(),available.bottom()-size.height()+1));
    m_normal=QRect(position,size);
    if(m_window->windowState()==Qt::WindowNoState)m_window->setGeometry(m_normal);
    m_timer.start();
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

#include "SystemEventFilter.h"
#include <QCoreApplication>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace daymark {
SystemEventFilter::SystemEventFilter(std::function<void()> refresh):m_refresh(std::move(refresh))
{
    m_pending.setSingleShot(true);m_pending.setInterval(50);
    connect(&m_pending,&QTimer::timeout,this,[this]{const auto callback=m_refresh;if(callback)callback();});
    if(QCoreApplication::instance())QCoreApplication::instance()->installNativeEventFilter(this);
}
void SystemEventFilter::requestRefresh(){if(!m_pending.isActive())m_pending.start();}
SystemEventFilter::~SystemEventFilter(){if(QCoreApplication::instance())QCoreApplication::instance()->removeNativeEventFilter(this);}
bool SystemEventFilter::nativeEventFilter(const QByteArray& eventType,void* message,qintptr* result)
{
    Q_UNUSED(result)
#ifdef Q_OS_WIN
    if(eventType=="windows_generic_MSG" || eventType=="windows_dispatcher_MSG") {
        const auto* native=static_cast<MSG*>(message);
        if(native && (native->message==WM_TIMECHANGE || native->message==WM_SETTINGCHANGE
            || (native->message==WM_POWERBROADCAST && (native->wParam==PBT_APMRESUMEAUTOMATIC || native->wParam==PBT_APMRESUMESUSPEND))))requestRefresh();
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
#endif
    return false;
}
}

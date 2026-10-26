#include "platform/common/NativeWindowLayer.h"
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QSet>
#include <QWindow>
#include <cwchar>
#include <windows.h>

namespace daymark {
namespace {
class DesktopLayerFilter final:public QAbstractNativeEventFilter {
public:
    DesktopLayerFilter(){QCoreApplication::instance()->installNativeEventFilter(this);}
    ~DesktopLayerFilter()override{if(QCoreApplication::instance())QCoreApplication::instance()->removeNativeEventFilter(this);}
    QSet<QWindow*> windows;
    bool nativeEventFilter(const QByteArray&,void* message,qintptr*)override {
        auto* msg=static_cast<MSG*>(message);
        if(!msg || msg->message!=WM_WINDOWPOSCHANGING)return false;
        for(auto* window:windows){
            if(!window->handle() || !window->flags().testFlag(Qt::WindowStaysOnBottomHint) || reinterpret_cast<HWND>(window->winId())!=msg->hwnd)continue;
            auto* position=reinterpret_cast<WINDOWPOS*>(msg->lParam);
            if(position && !(position->flags & SWP_NOZORDER)){position->hwndInsertAfter=HWND_BOTTOM;}
            break;
        }
        return false; // Preserve Qt/default size and position handling.
    }
};
DesktopLayerFilter& filter(){static DesktopLayerFilter instance;return instance;}
}
WindowResult applyNativeWindowLayer(QWindow* window,WindowLayer layer)
{
    if(!window || QGuiApplication::platformName()!=QStringLiteral("windows"))return {WindowError::WindowUnavailable};
    auto& guard=filter();
    if(layer==WindowLayer::Desktop && !guard.windows.contains(window)){
        guard.windows.insert(window);
        QObject::connect(window,&QObject::destroyed,[window]{filter().windows.remove(window);});
    }else if(layer!=WindowLayer::Desktop)guard.windows.remove(window);
    const auto handle=reinterpret_cast<HWND>(window->winId());
    const auto after=layer==WindowLayer::Desktop?HWND_BOTTOM:layer==WindowLayer::Topmost?HWND_TOPMOST:HWND_NOTOPMOST;
    return SetWindowPos(handle,after,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER)
        ?WindowResult{}:WindowResult{WindowError::OperationRejected};
}
bool nativeWindowLayerMatches(QWindow* window,WindowLayer layer)
{
    if(!window || !window->handle() || QGuiApplication::platformName()!=QStringLiteral("windows"))return false;
    const auto handle=reinterpret_cast<HWND>(window->winId());
    const bool top=(GetWindowLongPtrW(handle,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
    if(layer==WindowLayer::Topmost)return top;
    if(top)return false;
    if(layer==WindowLayer::Normal)return true;
    // A desktop window must have no visible ordinary top-level sibling below it.
    for(auto sibling=GetWindow(handle,GW_HWNDNEXT);sibling;sibling=GetWindow(sibling,GW_HWNDNEXT)){
        wchar_t name[64]{};GetClassNameW(sibling,name,64);
        if(IsWindowVisible(sibling) && std::wcscmp(name,L"Progman") && std::wcscmp(name,L"WorkerW"))return false;
    }
    return true;
}
}

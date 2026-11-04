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
bool restoreOwnedNativeMenus(HWND host)
{
    const auto thread=GetWindowThreadProcessId(host,nullptr);
    GUITHREADINFO info{};info.cbSize=sizeof(info);
    if(!GetGUIThreadInfo(thread,&info) || !(info.flags & GUI_INMENUMODE))return true;
    bool ownsMenu=false;
    for(auto owner=info.hwndMenuOwner;owner;owner=GetWindow(owner,GW_OWNER))if(owner==host){ownsMenu=true;break;}
    if(!ownsMenu)return true;
    QList<HWND> menus;
    EnumThreadWindows(thread,[](HWND window,LPARAM context)->BOOL{
        wchar_t name[32]{};GetClassNameW(window,name,32);
        if(IsWindowVisible(window) && std::wcscmp(name,L"#32768")==0)reinterpret_cast<QList<HWND>*>(context)->append(window);
        return TRUE;
    },reinterpret_cast<LPARAM>(&menus));
    // Native menus are absent from QGuiApplication::topLevelWindows(). Preserve
    // submenu order and keep the restored transparent host behind its menus.
    for(auto menu=menus.crbegin();menu!=menus.crend();++menu)
        if(!SetWindowPos(*menu,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER))return false;
    return true;
}
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
    return SetWindowPos(handle,after,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER) && restoreOwnedNativeMenus(handle)
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
bool nativeWindowAbove(QWindow* upper,QWindow* lower)
{
    if(!upper || !lower || !upper->handle() || !lower->handle() || QGuiApplication::platformName()!=QStringLiteral("windows"))return false;
    const auto top=reinterpret_cast<HWND>(upper->winId()),bottom=reinterpret_cast<HWND>(lower->winId());
    if(!IsWindowVisible(top) || !IsWindowVisible(bottom))return false;
    for(auto sibling=GetWindow(bottom,GW_HWNDPREV);sibling;sibling=GetWindow(sibling,GW_HWNDPREV))if(sibling==top)return true;
    return false;
}
bool nativeWindowRegionMatches(QWindow* window,const QRect& logicalBounds)
{
    if(!window || !window->handle() || QGuiApplication::platformName()!=QStringLiteral("windows"))return false;
    RECT bounds{};if(GetWindowRgnBox(reinterpret_cast<HWND>(window->winId()),&bounds)<=NULLREGION)return false;
    const auto scale=window->devicePixelRatio();
    return qAbs(bounds.left-qRound(logicalBounds.x()*scale))<=2 && qAbs(bounds.top-qRound(logicalBounds.y()*scale))<=2 &&
        qAbs((bounds.right-bounds.left)-qRound(logicalBounds.width()*scale))<=2 && qAbs((bounds.bottom-bounds.top)-qRound(logicalBounds.height()*scale))<=2;
}

}

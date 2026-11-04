#include "SmokeMenuControl.h"
#include <QWindow>
#ifdef Q_OS_WIN
#include <windows.h>
#include <map>
#include <cwchar>
namespace {
std::map<UINT_PTR,std::function<void()>> callbacks;
void CALLBACK dismiss(HWND window,UINT,UINT_PTR id,DWORD){
    KillTimer(window,id);
    auto node=callbacks.extract(id);
    if(!node.empty())node.mapped()();
    EndMenu();
}
}
#endif
namespace daymark {
bool smokeNativeMenuAbove(QWindow* owner){
#ifdef Q_OS_WIN
    if(!owner || !owner->handle())return false;
    for(auto window=GetWindow(reinterpret_cast<HWND>(owner->winId()),GW_HWNDPREV);window;window=GetWindow(window,GW_HWNDPREV)){
        wchar_t name[32]{};GetClassNameW(window,name,32);
        if(GetWindowThreadProcessId(window,nullptr)==GetCurrentThreadId() && IsWindowVisible(window) && std::wcscmp(name,L"#32768")==0)return true;
    }
#else
    Q_UNUSED(owner);
#endif
    return false;
}
bool smokeNativeMenuVisible(){
#ifdef Q_OS_WIN
    bool found=false;
    EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM context)->BOOL{
        wchar_t name[32]{};GetClassNameW(window,name,32);
        if(IsWindowVisible(window) && std::wcscmp(name,L"#32768")==0)*reinterpret_cast<bool*>(context)=true;
        return TRUE;
    },reinterpret_cast<LPARAM>(&found));
    return found;
#else
    return false;
#endif
}
bool scheduleSmokeMenuDismiss(QWindow* window,std::function<void()> callback){
#ifdef Q_OS_WIN
    if(!window || !window->handle())return false;
    static UINT_PTR next=0x444D7400;
    const auto id=++next;
    if(!SetTimer(reinterpret_cast<HWND>(window->winId()),id,120,dismiss))return false;
    callbacks.emplace(id,std::move(callback));return true;
#else
    Q_UNUSED(window);Q_UNUSED(callback);return false;
#endif
}
}

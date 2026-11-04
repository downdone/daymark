#include "SmokeMenuControl.h"
#include <QWindow>
#ifdef Q_OS_WIN
#include <windows.h>
#include <map>
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

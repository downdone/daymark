#include "platform/common/NativeWindowLayer.h"
#include <QWindow>
#include <QVariant>
#include <QGuiApplication>
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

namespace daymark {
namespace {
NSInteger levelFor(WindowLayer layer)
{
    if(layer==WindowLayer::Desktop)return CGWindowLevelForKey(kCGDesktopIconWindowLevelKey)+1;
    return layer==WindowLayer::Topmost?NSFloatingWindowLevel:NSNormalWindowLevel;
}
NSWindow* nativeWindow(QWindow* window)
{
    if(QGuiApplication::platformName()!=QStringLiteral("cocoa"))return nil;
    auto* view=reinterpret_cast<NSView*>(window->winId());
    return view.window;
}
}
WindowResult applyNativeWindowLayer(QWindow* window,WindowLayer layer)
{
    if(!window)return {WindowError::WindowUnavailable};
    @autoreleasepool {
        auto* native=nativeWindow(window);
        if(!native)return {WindowError::WindowUnavailable};
        if(!window->property("daymarkCollectionSaved").toBool()){
            window->setProperty("daymarkCollectionSaved",true);
            window->setProperty("daymarkOriginalCollection",qulonglong(native.collectionBehavior));
        }
        const auto original=NSWindowCollectionBehavior(window->property("daymarkOriginalCollection").toULongLong());
        native.collectionBehavior=layer==WindowLayer::Desktop
            ? (original & ~(NSWindowCollectionBehaviorMoveToActiveSpace | NSWindowCollectionBehaviorManaged | NSWindowCollectionBehaviorTransient))
                | NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary | NSWindowCollectionBehaviorIgnoresCycle
            : original;
        native.hidesOnDeactivate=NO;
        const bool popup=window->type()==Qt::Popup && layer==WindowLayer::Topmost;
        native.level=popup ? qMax(native.level,levelFor(layer)) : levelFor(layer);
        if(layer==WindowLayer::Desktop && native.visible)[native orderBack:nil];
        return (popup ? native.level>=levelFor(layer) : native.level==levelFor(layer))?WindowResult{}:WindowResult{WindowError::OperationRejected};
    }
}
bool nativeWindowLayerMatches(QWindow* window,WindowLayer layer)
{
    if(!window || !window->handle())return false;
    @autoreleasepool {auto* native=nativeWindow(window);return native && (window->type()==Qt::Popup && layer==WindowLayer::Topmost ? native.level>=levelFor(layer) : native.level==levelFor(layer));}
}
bool nativeWindowAbove(QWindow* upper,QWindow* lower)
{
    if(!upper || !lower || !upper->handle() || !lower->handle())return false;
    @autoreleasepool {
        auto* top=nativeWindow(upper);auto* bottom=nativeWindow(lower);
        if(!top || !bottom || !top.visible || !bottom.visible)return false;
        if(top.level!=bottom.level)return top.level>bottom.level;
        const auto windows=NSApp.orderedWindows;
        const auto topIndex=[windows indexOfObject:top],bottomIndex=[windows indexOfObject:bottom];
        return topIndex!=NSNotFound && bottomIndex!=NSNotFound && topIndex<bottomIndex;
    }
}
}

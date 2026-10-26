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
        native.level=levelFor(layer);
        if(layer==WindowLayer::Desktop && native.visible)[native orderBack:nil];
        return native.level==levelFor(layer)?WindowResult{}:WindowResult{WindowError::OperationRejected};
    }
}
bool nativeWindowLayerMatches(QWindow* window,WindowLayer layer)
{
    if(!window || !window->handle())return false;
    @autoreleasepool {auto* native=nativeWindow(window);return native && native.level==levelFor(layer);}
}
}

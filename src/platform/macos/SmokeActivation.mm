#include "SmokeActivation.h"
#include <QWindow>
#include <QDebug>
#import <AppKit/AppKit.h>

void daymark::activateMacSmokeWindow(QWindow* window)
{
    if(window) {
        window->showNormal();
        window->raise();
    }
    @autoreleasepool {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        const BOOL accepted=[[NSRunningApplication currentApplication]
            activateWithOptions:NSApplicationActivateAllWindows | NSApplicationActivateIgnoringOtherApps];
        if(!accepted)[NSApp activateIgnoringOtherApps:YES];
        if(window) {
            auto* view=reinterpret_cast<NSView*>(window->winId());
            [view.window makeKeyAndOrderFront:nil];
            static int attempts=0;
            if(++attempts<=2 || NSApp.active)qInfo()<<"Smoke native activation:"<<bool(accepted)<<bool(NSApp.active)<<bool(view.window.keyWindow)<<int(NSApp.activationPolicy)<<bool(NSRunningApplication.currentApplication);
        }
    }
    if(window)window->requestActivate();
}

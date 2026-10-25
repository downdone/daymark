#include "AccessibilityPreferences.h"
#include <QtCore/qglobal.h>
#ifdef Q_OS_MACOS
#import <AppKit/AppKit.h>
#elif defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
std::optional<bool> daymark::systemReducedMotion()
{
#ifdef Q_OS_MACOS
    @autoreleasepool {return bool(NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion);}
#elif defined(Q_OS_WIN)
    BOOL animations=TRUE;
    if(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0))return !animations;
    return std::nullopt;
#else
    return std::nullopt;
#endif
}

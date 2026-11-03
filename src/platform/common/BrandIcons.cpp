#include "BrandIcons.h"
#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>

namespace daymark::BrandIcons {
QIcon application()
{
    QIcon result;
    for(const int size:{16,20,24,32,48,64,128,256,512,1024})
        result.addFile(QStringLiteral(":/daymark/icons/daymark-%1.png").arg(size));
    return result;
}
QIcon tray(bool lightSurface)
{
    QIcon result;
    const auto color=lightSurface?QStringLiteral("black"):QStringLiteral("white");
    for(const int size:{16,20,24,32})
        result.addFile(QStringLiteral(":/daymark/icons/tray-%1-%2.png").arg(color).arg(size));
#ifdef Q_OS_MACOS
    // AppKit templates recolor the alpha silhouette for the actual menu bar.
    result.setIsMask(true);
#endif
    return result;
}
bool trayUsesLightSurface()
{
    const bool fallback=QGuiApplication::styleHints()->colorScheme()!=Qt::ColorScheme::Dark;
#ifdef Q_OS_WIN
    // The Windows taskbar theme can differ from the application theme.
    const QSettings personalization(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),QSettings::NativeFormat);
    return personalization.value(QStringLiteral("SystemUsesLightTheme"),fallback?1:0).toInt()!=0;
#else
    return fallback;
#endif
}
}

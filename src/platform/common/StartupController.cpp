#include "StartupController.h"
#include <QCoreApplication>
#include <QDir>

namespace daymark {
StartupController::StartupController(const QString& isolatedSettings,QObject* parent):QObject(parent)
{
    if(!isolatedSettings.isEmpty())m_settings=std::make_unique<QSettings>(isolatedSettings,QSettings::IniFormat);
#ifdef Q_OS_WIN
    else m_settings=std::make_unique<QSettings>(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),QSettings::NativeFormat);
#endif
    m_enabled=m_settings && !m_settings->value(QStringLiteral("Daymark")).toString().isEmpty();
}
bool StartupController::supported()const{return bool(m_settings);}
bool StartupController::enabled()const{return m_enabled;}
bool StartupController::setEnabled(bool value)
{
    m_error.clear();
    if(!m_settings){m_error=tr("此平台尚未提供开机启动适配，请通过系统登录项设置。");emit changed();return false;}
    // Only this explicit user action changes the user-level startup registration.
    if(value)m_settings->setValue(QStringLiteral("Daymark"),QLatin1Char('"')+QDir::toNativeSeparators(QCoreApplication::applicationFilePath())+QLatin1Char('"'));
    else m_settings->remove(QStringLiteral("Daymark"));
    m_settings->sync();
    if(m_settings->status()!=QSettings::NoError){m_error=tr("无法更新开机启动。请检查当前用户权限或系统启动项策略后重试。");emit changed();return false;}
    m_enabled=value;emit changed();return true;
}
}

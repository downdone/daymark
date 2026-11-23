#pragma once
#include "Protocol.h"
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>

namespace daymark::sync {
class ProfileIndex final {
  public:
    explicit ProfileIndex(QString root) : root(QDir(root).absolutePath()) {}
    QString activePath() const {
        QSettings s(file(), QSettings::IniFormat);
        const auto relative =
            s.value("active_database", QStringLiteral("daymark.sqlite")).toString();
        return valid(relative) ? QDir(root).filePath(relative)
                               : QDir(root).filePath("daymark.sqlite");
    }
    QString pathFor(const QString &server, const QString &instance, const QString &user) const {
        QSettings settings(file(), QSettings::IniFormat);
        settings.beginGroup("profiles");
        for (const auto &id : settings.childGroups()) {
            settings.beginGroup(id);
            const auto path = settings.value("path").toString();
            const bool matches = settings.value("origin").toString() == server &&
                                 settings.value("instance").toString() == instance &&
                                 settings.value("user").toString() == user;
            settings.endGroup();
            if (matches && path != localRelative() && valid(path) && QFileInfo::exists(QDir(root).filePath(path)))
                return QDir(root).filePath(path);
        }
        return QDir(root).filePath(
            "profiles/" +
            QString::fromLatin1(
                QCryptographicHash::hash((server + "/" + instance + "/" + user).toUtf8(),
                                         QCryptographicHash::Sha256)
                    .toHex()) +
            "/daymark.sqlite");
    }
    bool isLocalOnly(const QString &path) const {
        return QDir(root).relativeFilePath(path) == localRelative();
    }
    Result<void> remember(const QString &path, const QJsonObject &profile) const {
        const auto relative = QDir(root).relativeFilePath(path);
        if (!valid(relative))
            return Result<void>::failure(ErrorCode::PermissionError, "syncProfilePath");
        QSettings s(file(), QSettings::IniFormat);
        const auto id = QString::fromLatin1(
            QCryptographicHash::hash(relative.toUtf8(), QCryptographicHash::Sha256).toHex());
        s.setValue("profiles/" + id + "/path", relative);
        const auto label = isLocalOnly(path) ? QCoreApplication::translate("AccountWindow", "仅本机资料")
                                            : profile.value("login_label").toString(QStringLiteral("本机资料"));
        s.setValue("profiles/" + id + "/label", label);
        s.setValue("profiles/" + id + "/origin", profile.value("server_origin").toString());
        s.setValue("profiles/" + id + "/instance", profile.value("server_instance_id").toString());
        s.setValue("profiles/" + id + "/user", profile.value("user_id").toString());
        s.setValue("active_database", relative);
        s.sync();
        return s.status() == QSettings::NoError
                   ? Result<void>{}
                   : Result<void>::failure(ErrorCode::PersistenceError, "settingsWrite");
    }
    QJsonArray entries() const {
        QSettings s(file(), QSettings::IniFormat);
        s.beginGroup("profiles");
        QJsonArray result;
        for (const auto &id : s.childGroups()) {
            s.beginGroup(id);
            const auto relative = s.value("path").toString();
            if (valid(relative) && QFileInfo::exists(QDir(root).filePath(relative)))
                result.append(QJsonObject{{"id", id},
                                          {"label", s.value("label").toString()},
                                          {"origin", s.value("origin").toString()}});
            s.endGroup();
        }
        return result;
    }
    QString selectedPath(const QString &id) const {
        if (id == QStringLiteral("local") && valid(localRelative()))
            return QDir(root).filePath(localRelative());
        if (id.size() != 64)
            return {};
        QSettings s(file(), QSettings::IniFormat);
        const auto relative = s.value("profiles/" + id + "/path").toString();
        return valid(relative) ? QDir(root).filePath(relative) : QString();
    }

  private:
    QString root;
    static QString localRelative() { return QStringLiteral("profiles/local/daymark.sqlite"); }
    QString file() const {
        return QDir(root).filePath("sync-device.ini");
    }
    bool valid(const QString &relative) const {
        static const QRegularExpression re(
            QStringLiteral("^(daymark\\.sqlite|profiles/(?:local|[0-9a-f]{64})/daymark\\.sqlite)$"));
        if (!re.match(relative).hasMatch())
            return false;
        const QFileInfo database(QDir(root).filePath(relative));
        const QFileInfo localDirectory(database.dir().absolutePath());
        const QFileInfo profilesDirectory(QDir(root).filePath("profiles"));
        if (relative == localRelative() &&
            (database.isSymLink() || localDirectory.isSymLink() || profilesDirectory.isSymLink()))
            return false;
        const auto parent = database.dir().canonicalPath();
        const auto canonicalRoot = QFileInfo(root).canonicalFilePath();
        return parent.isEmpty() ||
               (!canonicalRoot.isEmpty() &&
                (parent == canonicalRoot ||
                 parent.startsWith(canonicalRoot + QLatin1Char('/'), Qt::CaseInsensitive)));
    }
};
} // namespace daymark::sync

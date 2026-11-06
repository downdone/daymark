#include "UpdateController.h"
#include "diagnostics/RuntimeLog.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QTimer>
#include <QtEndian>
#include <optional>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif
namespace daymark {
namespace {
std::optional<QJsonObject> readState(const QString &path) {
    QFile file(path);
    if (!file.exists())
        return QJsonObject{};
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const auto bytes = file.read(131073);
    file.close();
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (bytes.size() > 131072 || error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    return document.object();
}
bool stableVersion(const QString &value) {
    static const QRegularExpression pattern(
        QStringLiteral("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$"));
    return value.size() <= 128 && pattern.match(value).hasMatch();
}
} // namespace
UpdateController::UpdateController(QString data, bool verification, QObject *parent)
    : QObject(parent), m_data(std::move(data)), m_verification(verification) {
#ifdef Q_OS_WIN
    if (!verification) {
        auto folder = QFileInfo(QCoreApplication::applicationFilePath()).dir();
        auto parent = folder;
        parent.cdUp();
        if (parent.dirName() == "versions") {
            parent.cdUp();
            if (QFileInfo::exists(parent.filePath("daymark-update-cli.exe")))
                m_root = parent.canonicalPath();
        }
    }
#endif
    if (!supported())
        return;
    const auto controlName =
        QStringLiteral("daymark-update-control-") +
        QString::fromLatin1(
            QCryptographicHash::hash(m_root.toLower().toUtf8(), QCryptographicHash::Sha256)
                .toHex()
                .left(32));
    m_control.setSocketOptions(QLocalServer::UserAccessOption);
    m_control.listen(controlName);
    connect(&m_control, &QLocalServer::newConnection, this, [this] {
        while (auto *socket = m_control.nextPendingConnection()) {
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket, buffer] {
                *buffer += socket->readAll();
                if (buffer->size() < 4)
                    return;
                auto length = qFromLittleEndian<quint32>(
                    reinterpret_cast<const uchar *>(buffer->constData()));
                if (length > 65536) {
                    socket->disconnectFromServer();
                    return;
                }
                if (buffer->size() < 4 + length)
                    return;
                const auto command = QJsonDocument::fromJson(buffer->mid(4, length)).object();
                bool accepted = command.value("type").toString() == "RESTART" && ready();
                auto bytes = QJsonDocument(QJsonObject{{"accepted", accepted}})
                                 .toJson(QJsonDocument::Compact);
                QByteArray frame(4, 0);
                qToLittleEndian<quint32>(quint32(bytes.size()),
                                         reinterpret_cast<uchar *>(frame.data()));
                frame += bytes;
                socket->write(frame);
                socket->flush();
                socket->disconnectFromServer();
                if (accepted)
                    QTimer::singleShot(0, this, &UpdateController::restart);
            });
        }
    });
    m_watch.addPath(QDir(m_root).filePath("update-state"));
    connect(&m_watch, &QFileSystemWatcher::directoryChanged, this, [this] { reload(); });
    connect(&m_worker, &QProcess::readyReadStandardOutput, this, [this] {
        m_worker.readAllStandardOutput();
        reload();
    });
    connect(&m_worker, &QProcess::readyReadStandardError, this,
            [this] { m_worker.readAllStandardError(); });
    connect(&m_worker, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus) {
                reload();
                emit changed();
                if (m_restartQueued && ready()) {
                    m_restartQueued = false;
                    restart();
                }
            });
    connect(&m_worker, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError) { localFailure("update.start_failed"); });
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        if (!m_restart)
            return;
#ifdef Q_OS_WIN
        FILETIME created, exit, kernel, user;
        if (!GetProcessTimes(GetCurrentProcess(), &created, &exit, &kernel, &user))
            return;
        auto stamp =
            QString::number((quint64(created.dwHighDateTime) << 32) | created.dwLowDateTime);
        QStringList args{"apply",       "--root",    m_root,
                         "--json",      "--old-pid", QString::number(GetCurrentProcessId()),
                         "--old-stamp", stamp,       "--data-dir",
                         m_data};
        if (!QProcess::startDetached(QDir(m_root).filePath("daymark-update-cli.exe"), args, m_root))
            m_state["error"] = "update.start_failed";
#endif
    });
    reload();
}
UpdateController::~UpdateController() {
    if (busy()) {
        cancel();
        m_worker.waitForFinished(5000);
    }
}
void UpdateController::reload() {
    const auto directory = QDir(m_root);
    const auto status = readState(directory.filePath("update-state/status.json"));
    const auto preferences = readState(directory.filePath("update-state/preferences.json"));
    const auto prepared = readState(directory.filePath("update-state/prepared-update.json"));
    const bool exists = QFileInfo::exists(directory.filePath("update-state/prepared-update.json"));
    // Display validation only. The native SDK remains the signature/security authority.
    if (!status || !preferences || !prepared ||
        (exists && (prepared->value("format").toInt() != 1 ||
                    prepared->value("transaction_id").toString().isEmpty() ||
                    !stableVersion(prepared->value("version").toString())))) {
        m_state = {
            {"auto_update", preferences ? preferences->value("auto_update").toBool(true) : false}};
        localFailure("update.state_unavailable");
        return;
    }
    auto state = *status;
    state["ready"] = exists;
    state["auto_update"] = preferences->value("auto_update").toBool(true);
    if (exists)
        state["version"] = prepared->value("version");
    if (m_localError == "update.state_unavailable")
        m_localError.clear();
    m_state = state;
    emit changed();
    const auto candidate = prepared->value("transaction_id").toString();
    if (ready() && candidate != m_notifiedCandidate) {
        m_notifiedCandidate = candidate;
        QTimer::singleShot(0, this, [this, candidate] {
            if (ready() && m_notifiedCandidate == candidate)
                emit updateReady();
        });
    }
}
QString UpdateController::statusText() const {
    if (m_localError == "update.state_unavailable")
        return tr("无法读取更新状态，请保留安装目录并检查权限；可从发布说明手动升级。");
    if (m_localError == "update.start_failed")
        return tr("无法启动更新操作，请检查更新器文件是否完整。");
    if (m_localError == "update.browser_failed")
        return tr("无法打开发布说明，请检查系统默认浏览器。");
    if (!supported())
        return tr("自动更新需要使用带启动器的便携包。");
    const auto error = m_state.value("error").toString();
    if (!error.isEmpty()) {
        if (error == "update.trust_not_configured")
            return tr("当前构建尚未配置更新信任公钥。");
        if (error == "update.network_unavailable" || error == "update.timeout")
            return tr("无法连接更新源，稍后可重试；本地任务不受影响。");
        if (error == "update.assets_pending")
            return tr("新版 Windows 更新资源尚未就绪。");
        if (error == "update.rate_limited")
            return tr("更新源暂时限流，请稍后重试。");
        if (error == "update.cancelled")
            return tr("已取消本次更新准备。");
        if (error == "update.signature_invalid" || error == "update.hash_mismatch")
            return tr("更新验证失败，已停止安装。");
        if (error == "update.data_compatibility" || error == "update.incompatible_runtime" ||
            error == "update.bootstrap_too_old")
            return tr("此更新需要手动升级，请查看发布说明。");
        return tr("更新未完成，可重试或查看发布说明。（%1）").arg(error);
    }
    if (ready())
        return tr("更新已安装，重启后生效。");
    const auto state = m_state.value("state").toString();
    if (state == "checking")
        return tr("正在检查更新…");
    if (state == "downloading")
        return tr("正在下载更新…");
    if (state == "verifying")
        return tr("正在验证更新…");
    if (state == "staging")
        return tr("正在安装新版本…");
    if (state == "up_to_date")
        return tr("已是最新可安装版本。");
    if (state == "deferred")
        return tr("已暂缓更新检查。");
    return tr("启动时自动检查并准备更新。");
}
double UpdateController::progress() const {
    auto total = m_state.value("total").toDouble();
    return total > 0 ? qBound(0., m_state.value("received").toDouble() / total, 1.) : -1.;
}
void UpdateController::check(bool automatic) {
    if (!supported() || busy() || ready())
        return;
    m_localError.clear();
    diagnostics::event("update", "prepare.requested", {{"automatic", automatic}});
    m_worker.setProgram(QDir(m_root).filePath("daymark-update-cli.exe"));
    QStringList args{"prepare", "--root", m_root, "--json"};
    if (automatic)
        args << "--auto";
    m_worker.setArguments(args);
    m_worker.start();
    emit changed();
}
void UpdateController::command(const QStringList &args) {
    if (!supported())
        return;
    m_localError.clear();
    diagnostics::event("update", "command.requested", {{"command", args.value(0)}});
    auto *process = new QProcess(this);
    process->setProgram(QDir(m_root).filePath("daymark-update-cli.exe"));
    process->setArguments(args + QStringList{"--root", m_root, "--json"});
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process](int, QProcess::ExitStatus) {
                reload();
                process->deleteLater();
            });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        localFailure("update.start_failed");
        if (error == QProcess::FailedToStart)
            process->deleteLater();
    });
    process->start();
}
void UpdateController::cancel() { command({"cancel"}); }
void UpdateController::cancelReady() { command({"cancel-ready"}); }
void UpdateController::setAutomatic(bool enabled) {
    command({"auto", "--enabled", enabled ? "true" : "false"});
}
void UpdateController::restart() {
    if (!ready())
        return;
    diagnostics::event("update", "restart.requested");
    if (busy()) {
        m_restartQueued = true;
        return;
    }
    emit restartRequested();
}
void UpdateController::showUpdater() {
    if (supported() &&
        !QProcess::startDetached(QDir(m_root).filePath("daymark-updater.exe"), {}, m_root))
        localFailure("update.start_failed");
}
void UpdateController::localFailure(const QString &key) {
    m_localError = key;
    diagnostics::event("update", "adapter.failed", {{"error_key", key}}, "warning");
    emit changed();
}
QUrl UpdateController::releaseNotesUrl() const {
    const auto version = availableVersion();
    return QUrl(QStringLiteral("https://github.com/downdone/daymark/releases") +
                (stableVersion(version) ? QStringLiteral("/tag/v") + version : QString()));
}
bool UpdateController::openReleaseNotes() {
    if (QDesktopServices::openUrl(releaseNotesUrl()))
        return true;
    localFailure("update.browser_failed");
    return false;
}
void UpdateController::runtimeReady() {
    if (m_started || m_verification)
        return;
    m_started = true;
    QTimer::singleShot(1000, this, [this] {
        if (!supported())
            return;
        QFile f(QDir(m_root).filePath("update-state/journal.json"));
        if (f.open(QIODevice::ReadOnly) &&
            QJsonDocument::fromJson(f.read(131072)).object().value("phase").toString() ==
                "committed")
            command({"recover"});
    });
    QTimer::singleShot(3000, this, [this] {
        if (automatic())
            check(true);
    });
}
} // namespace daymark

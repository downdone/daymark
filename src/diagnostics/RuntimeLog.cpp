#include "RuntimeLog.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QMutex>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>
#include <atomic>
#include <cstdio>
#include <memory>

namespace daymark::diagnostics {
namespace {
struct State {
    QMutex mutex;
    QString directory, file, base, session, error;
    LogPolicy policy;
    QElapsedTimer elapsed;
    std::unique_ptr<QLockFile> lease;
    std::atomic<QtMessageHandler> previous{nullptr};
    bool installed = false;
};
State &state() {
    static State value;
    return value;
}
bool forbidden(const QString &key) {
    static const QStringList names{
        "password",     "old_password",  "new_password",  "confirm_password", "secret",   "token",
        "access_token", "refresh_token", "authorization", "cookie",           "login",    "account",
        "title",        "notes",         "body",          "payload",          "response", "headers",
        "url",          "origin",        "database_path", "device_name"};
    return names.contains(key.toLower());
}
QJsonValue clean(const QJsonValue &value, int &remaining, int depth = 0) {
    if (depth > 4 || --remaining < 0)
        return QStringLiteral("<limit>");
    if (value.isString())
        return redact(value.toString());
    if (value.isObject()) {
        QJsonObject result;
        int count = 0;
        const auto input = value.toObject();
        for (auto it = input.begin(); it != input.end() && count < 40 && remaining > 0;
             ++it, ++count)
            if (!forbidden(it.key()))
                result.insert(it.key().left(80), clean(it.value(), remaining, depth + 1));
        return result;
    }
    if (value.isArray()) {
        QJsonArray result;
        const auto input = value.toArray();
        for (qsizetype i = 0; i < qMin<qsizetype>(32, input.size()) && remaining > 0; ++i)
            result.append(clean(input[i], remaining, depth + 1));
        return result;
    }
    return value;
}
QJsonObject record(const QString &component, const QString &name, const QJsonObject &fields,
                   const QString &level) {
    auto &s = state();
    int remaining = 256;
    return {{"format", 1},
            {"at_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"elapsed_ms", s.elapsed.isValid() ? s.elapsed.elapsed() : 0},
            {"level", level},
            {"component", component.left(80)},
            {"event", name.left(100)},
            {"pid", QCoreApplication::applicationPid()},
            {"session", s.session},
            {"thread", QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16)},
            {"fields", clean(fields, remaining)}};
}
void append(const QJsonObject &value) {
    auto &s = state();
    if (s.file.isEmpty())
        return;
    if (QFileInfo(s.file).isSymLink()) {
        s.error = QStringLiteral("diagnostics.unsafe_path");
        return;
    }
    if (QFileInfo(s.file).size() >= s.policy.maximumFileBytes) {
        const auto previous =
            QDir(s.directory).filePath(s.base + QStringLiteral(".previous.jsonl"));
        if (QFileInfo(previous).isSymLink()) {
            s.error = QStringLiteral("diagnostics.unsafe_path");
            return;
        }
        if (QFile::exists(previous) && !QFile::remove(previous)) {
            s.error = QStringLiteral("diagnostics.rotate_failed");
            return;
        }
        if (!QFile::rename(s.file, previous)) {
            s.error = QStringLiteral("diagnostics.rotate_failed");
            return;
        }
        QFile header(s.file);
        if (header.open(QIODevice::WriteOnly)) {
            header.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            header.write(QJsonDocument(record("logger", "session.rotated", {}, "info"))
                             .toJson(QJsonDocument::Compact) +
                         '\n');
        }
    }
    QFile file(s.file);
    const bool fresh = !file.exists();
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        s.error = QStringLiteral("diagnostics.write_failed");
        return;
    }
    if (fresh)
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    auto bytes = QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n';
    if (bytes.size() > 16384) {
        auto bounded = value;
        bounded["fields"] = QJsonObject{{"truncated", true}};
        bytes = QJsonDocument(bounded).toJson(QJsonDocument::Compact) + '\n';
    }
    if (file.write(bytes) != bytes.size() || !file.flush())
        s.error = QStringLiteral("diagnostics.write_failed");
}
void prune() {
    auto &s = state();
    const auto entries =
        QDir(s.directory)
            .entryInfoList({"daymark-*.jsonl"}, QDir::Files | QDir::NoSymLinks, QDir::Time);
    const QRegularExpression owned(
        QStringLiteral("^(daymark-\\d{8}T\\d{6}Z-\\d+-[0-9a-f]{8})(?:\\.previous)?\\.jsonl$"));
    int retained = 0;
    for (const auto &item : entries) {
        const auto match = owned.match(item.fileName());
        if (!match.hasMatch())
            continue;
        const bool expired = item.lastModified().toUTC() <
                             QDateTime::currentDateTimeUtc().addDays(-s.policy.retainedDays);
        if (!expired && retained++ < s.policy.retainedFiles)
            continue;
        QLockFile lease(QDir(s.directory).filePath(match.captured(1) + ".lock"));
        if (!lease.tryLock(0))
            continue;
        QFile file(item.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const auto header = QJsonDocument::fromJson(file.readLine(8192)).object();
        file.close();
        if (header.value("format").toInt() == 1 &&
            header.value("component").toString() == "logger" &&
            header.value("event").toString().startsWith("session."))
            QFile::remove(item.absoluteFilePath());
    }
}
void handler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    static thread_local bool inside = false;
    auto &s = state();
    if (!inside && type != QtDebugMsg) {
        inside = true;
        {
            QMutexLocker lock(&s.mutex);
            const auto level = type == QtFatalMsg      ? "fatal"
                               : type == QtCriticalMsg ? "error"
                               : type == QtWarningMsg  ? "warning"
                                                       : "info";
            append(
                record("qt", QString::fromLatin1(context.category ? context.category : "default"),
                       {{"message", redact(message)},
                        {"source",
                         QFileInfo(QString::fromUtf8(context.file ? context.file : "")).fileName()},
                        {"line", context.line}},
                       QString::fromLatin1(level)));
        }
        inside = false;
    }
    if (const auto previous = s.previous.load())
        previous(type, context, message);
    else {
        const auto bytes = message.toUtf8();
        std::fprintf(stderr, "%s\n", bytes.constData());
        std::fflush(stderr);
    }
}
} // namespace
QString redact(QString text) {
    static const QRegularExpression controls(QStringLiteral("[\\x00-\\x08\\x0b-\\x1f\\x7f]"));
    static const QRegularExpression urls(QStringLiteral("(?i)(?:https?|socks5?)://[^\\s<>\\\"]+"));
    static const QRegularExpression jsonSecrets(QStringLiteral(
        R"re((?i)"(?:password|(?:access_|refresh_)?token|authorization|cookie|secret)"\s*:\s*"(?:\\.|[^"\\])*")re"));
    static const QRegularExpression secrets(
        QStringLiteral("(?i)\\b(?:authorization|cookie|password|(?:access_|refresh_)?token|secret)"
                       "\\s*[:=]\\s*(?:Bearer\\s+)?[^\\s,;]+"));
    static const QRegularExpression emails(
        QStringLiteral("[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}"));
    text = text.left(8192);
    text.replace(controls, " ");
    text.replace(urls, "<endpoint>");
    text.replace(jsonSecrets, "\"secret\":\"<redacted>\"");
    text.replace(secrets, "<redacted-secret>");
    static const auto home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home.isEmpty()) {
        text.replace(home, "<user-home>", Qt::CaseInsensitive);
        text.replace(QDir::toNativeSeparators(home), "<user-home>", Qt::CaseInsensitive);
    }
    text.replace(emails, "<email>");
    return text.left(2048);
}
bool start(const QString &directory, LogPolicy policy) {
    stop();
    auto &s = state();
    QMutexLocker lock(&s.mutex);
    s.error.clear();
    if (directory.isEmpty() || QFileInfo(directory).isSymLink() || !QDir().mkpath(directory)) {
        s.error = "diagnostics.directory_unavailable";
        return false;
    }
    s.directory = QDir(directory).absolutePath();
    s.policy = policy;
    s.policy.maximumFileBytes = qBound<qint64>(1024LL, policy.maximumFileBytes, 2LL * 1024 * 1024);
    s.policy.retainedFiles = qBound(1, policy.retainedFiles, 10);
    s.policy.retainedDays = qBound(1, policy.retainedDays, 7);
    s.session = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    s.elapsed.start();
    s.base = "daymark-" + QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmssZ") + "-" +
             QString::number(QCoreApplication::applicationPid()) + "-" + s.session;
    s.file = QDir(s.directory).filePath(s.base + ".jsonl");
    s.lease = std::make_unique<QLockFile>(QDir(s.directory).filePath(s.base + ".lock"));
    if (!s.lease->tryLock(0)) {
        s.error = "diagnostics.lock_failed";
        s.file.clear();
        s.lease.reset();
        return false;
    }
    append(record("logger", "session.started", {}, "info"));
    prune();
    if (!s.error.isEmpty()) {
        s.file.clear();
        s.lease.reset();
        return false;
    }
    s.previous.store(qInstallMessageHandler(handler));
    s.installed = true;
    return true;
}
void stop() {
    auto &s = state();
    QMutexLocker lock(&s.mutex);
    if (s.installed) {
        qInstallMessageHandler(s.previous.load());
        s.installed = false;
    }
    if (!s.file.isEmpty())
        append(record("logger", "session.stopped", {}, "info"));
    s.file.clear();
    s.lease.reset();
    s.directory.clear();
}
QString directory() {
    auto &s = state();
    QMutexLocker lock(&s.mutex);
    return s.directory;
}
QString error() {
    auto &s = state();
    QMutexLocker lock(&s.mutex);
    return s.error;
}
void event(const QString &component, const QString &name, const QJsonObject &fields,
           const QString &level) {
    auto &s = state();
    QMutexLocker lock(&s.mutex);
    if (!s.file.isEmpty())
        append(record(component, name, fields, level));
}
} // namespace daymark::diagnostics

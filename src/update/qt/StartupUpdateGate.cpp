#include "StartupUpdateGate.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#ifdef Q_OS_WIN
#include "update/platform/windows/Ipc.h"
#endif

namespace daymark {
struct StartupUpdateGate::Impl {
    QString root, error, instanceFile, version;
    bool recordedRunning = false;
    QJsonObject context;
#ifdef Q_OS_WIN
    HANDLE lease = INVALID_HANDLE_VALUE, pipe = INVALID_HANDLE_VALUE;
    OVERLAPPED lock{};
    ~Impl() {
        if (!instanceFile.isEmpty())
            QFile::remove(instanceFile);
        if (pipe != INVALID_HANDLE_VALUE)
            CloseHandle(pipe);
        if (lease != INVALID_HANDLE_VALUE) {
            UnlockFileEx(lease, 0, 1, 0, &lock);
            CloseHandle(lease);
        }
    }
    QJsonObject receive() {
        const auto bytes = update::ipc::receive(pipe, 90000);
        QJsonParseError parse;
        const auto j = QJsonDocument::fromJson(QByteArray::fromStdString(bytes), &parse);
        if (parse.error != QJsonParseError::NoError || !j.isObject())
            throw std::runtime_error("update.ipc_failed");
        return j.object();
    }
    void send(const QString &type) {
        QJsonObject value{{"type", type}, {"nonce", context.value("nonce")}};
        update::ipc::send(pipe, QJsonDocument(value).toJson(QJsonDocument::Compact).toStdString());
    }
#endif
};
StartupUpdateGate::StartupUpdateGate() : d(std::make_unique<Impl>()) {}
StartupUpdateGate::~StartupUpdateGate() = default;
QString StartupUpdateGate::root() const { return d->root; }
QString StartupUpdateGate::error() const { return d->error; }
bool StartupUpdateGate::restricted() const { return !d->context.isEmpty(); }
bool StartupUpdateGate::acquire(const QString &handoff, const QString &version) {
    d->version = version;
#ifdef Q_OS_WIN
    try {
        const auto image = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
        auto folder = QFileInfo(image).dir();
        if (folder.dirName().isEmpty())
            return true;
        auto parent = folder;
        parent.cdUp();
        if (parent.dirName() != QStringLiteral("versions")) {
            if (!handoff.isEmpty())
                throw std::runtime_error("update.process_identity");
            return true;
        }
        parent.cdUp();
        d->root = parent.canonicalPath();
        if (d->root.isEmpty())
            throw std::runtime_error("update.process_identity");
        if (!handoff.isEmpty()) {
            bool ok = false;
            const auto number = handoff.toULongLong(&ok);
            if (!ok || !number)
                throw std::runtime_error("update.process_identity");
            HANDLE mapping = reinterpret_cast<HANDLE>(static_cast<quintptr>(number));
            auto *bytes =
                static_cast<const char *>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 16384));
            if (!bytes)
                throw std::runtime_error("update.process_identity");
            auto length = std::find(bytes, bytes + 16384, '\0') - bytes;
            auto json = QJsonDocument::fromJson(QByteArray(bytes, static_cast<qsizetype>(length)));
            UnmapViewOfFile(bytes);
            CloseHandle(mapping);
            if (length >= 16384 || !json.isObject())
                throw std::runtime_error("update.process_identity");
            d->context = json.object();
            if (d->context.value("protocol").toInt() != 1 ||
                QDir(d->context.value("root").toString()).canonicalPath() != d->root ||
                d->context.value("version").toString() != version ||
                QDir(d->root).filePath(d->context.value("payload_dir").toString() +
                                       "/daymark.exe") != image)
                throw std::runtime_error("update.process_identity");
            const auto name = d->context.value("pipe").toString();
            if (!name.startsWith(QStringLiteral("\\\\.\\pipe\\daymark-update-")) ||
                !WaitNamedPipeW(reinterpret_cast<const wchar_t *>(name.utf16()), 10000))
                throw std::runtime_error("update.ipc_failed");
            d->pipe = CreateFileW(reinterpret_cast<const wchar_t *>(name.utf16()),
                                  GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED, nullptr);
            if (d->pipe == INVALID_HANDLE_VALUE)
                throw std::runtime_error("update.ipc_failed");
            ULONG pid = 0;
            if (!GetNamedPipeServerProcessId(d->pipe, &pid) ||
                pid != d->context.value("worker_pid").toVariant().toUInt())
                throw std::runtime_error("update.process_identity");
            HANDLE worker = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!worker)
                throw std::runtime_error("update.process_identity");
            wchar_t workerImage[32768];
            DWORD size = 32768;
            FILETIME created, exit, kernel, user;
            bool valid = QueryFullProcessImageNameW(worker, 0, workerImage, &size) &&
                         GetProcessTimes(worker, &created, &exit, &kernel, &user);
            CloseHandle(worker);
            if (!valid)
                throw std::runtime_error("update.process_identity");
            const auto stamp =
                QString::number((quint64(created.dwHighDateTime) << 32) | created.dwLowDateTime);
            const auto actual =
                QFileInfo(QString::fromWCharArray(workerImage, size)).canonicalFilePath();
            if (stamp != d->context.value("worker_stamp").toString() ||
                actual !=
                    QFileInfo(QDir(d->root).filePath("daymark-update-cli.exe")).canonicalFilePath())
                throw std::runtime_error("update.process_identity");
            auto hello = QJsonObject{
                {"type", "HELLO"}, {"nonce", d->context.value("nonce")}, {"version", version}};
            update::ipc::send(d->pipe,
                              QJsonDocument(hello).toJson(QJsonDocument::Compact).toStdString());
            auto accept = d->receive();
            if (accept.value("type").toString() != "ACCEPT" ||
                accept.value("nonce") != d->context.value("nonce"))
                throw std::runtime_error("update.process_identity");
        }
        auto lockPath = QDir(d->root).filePath("update-state/locks");
        QDir().mkpath(lockPath);
        HANDLE gate = INVALID_HANDLE_VALUE;
        OVERLAPPED gateLock{};
        if (!restricted()) {
            auto path = QDir(lockPath).filePath("launch");
            gate = CreateFileW(reinterpret_cast<const wchar_t *>(path.utf16()),
                               GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_ALWAYS, 0, nullptr);
            if (gate == INVALID_HANDLE_VALUE ||
                !LockFileEx(gate, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0,
                            &gateLock)) {
                if (gate != INVALID_HANDLE_VALUE)
                    CloseHandle(gate);
                throw std::runtime_error("update.instance_busy");
            }
            QFile file(QDir(d->root).filePath("current.json"));
            if (!file.open(QIODevice::ReadOnly)) {
                CloseHandle(gate);
                throw std::runtime_error("update.recovery_required");
            }
            auto current = QJsonDocument::fromJson(file.read(65536)).object();
            auto expected = QFileInfo(QDir(d->root).filePath(current.value("current").toString() +
                                                             "/daymark.exe"))
                                .canonicalFilePath();
            if (expected != image) {
                CloseHandle(gate);
                throw std::runtime_error("update.stale_payload");
            }
        }
        auto lease = QDir(lockPath).filePath("runtime");
        d->lease = CreateFileW(reinterpret_cast<const wchar_t *>(lease.utf16()),
                               GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_ALWAYS, 0, nullptr);
        bool leased = d->lease != INVALID_HANDLE_VALUE &&
                      LockFileEx(d->lease, LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &d->lock);
        if (gate != INVALID_HANDLE_VALUE) {
            UnlockFileEx(gate, 0, 1, 0, &gateLock);
            CloseHandle(gate);
        }
        if (!leased)
            throw std::runtime_error("update.instance_busy");
        const auto instances = QDir(d->root).filePath("update-state/instances");
        QDir().mkpath(instances);
        d->instanceFile =
            QDir(instances).filePath(QString::number(GetCurrentProcessId()) + ".json");
        FILETIME created, exit, kernel, user;
        if (!GetProcessTimes(GetCurrentProcess(), &created, &exit, &kernel, &user))
            throw std::runtime_error("update.process_identity");
        QSaveFile record(d->instanceFile);
        record.setDirectWriteFallback(false);
        const auto bytes =
            QJsonDocument(
                QJsonObject{{"pid", qint64(GetCurrentProcessId())},
                            {"stamp", QString::number((quint64(created.dwHighDateTime) << 32) |
                                                      created.dwLowDateTime)},
                            {"image", image},
                            {"version", version}})
                .toJson(QJsonDocument::Compact);
        if (!record.open(QIODevice::WriteOnly) || record.write(bytes) != bytes.size() ||
            !record.commit())
            throw std::runtime_error("update.process_identity");
        return true;
    } catch (const std::exception &e) {
        d->error = QString::fromUtf8(e.what());
        return false;
    }
#else
    Q_UNUSED(handoff);
    Q_UNUSED(version);
    return true;
#endif
}
bool StartupUpdateGate::commitReady() {
#ifdef Q_OS_WIN
    if (!restricted())
        return true;
    try {
        d->send("READY");
        try {
            auto reply = d->receive();
            if (reply.value("type").toString() != "COMMIT" ||
                reply.value("nonce") != d->context.value("nonce"))
                throw std::runtime_error("update.ipc_failed");
        } catch (const std::exception
                     &) { /* Durable commit is authoritative even if the reply was lost. */
        }
        QFile file(QDir(d->root).filePath("current.json"));
        if (!file.open(QIODevice::ReadOnly))
            throw std::runtime_error("update.recovery_required");
        auto current = QJsonDocument::fromJson(file.read(65536)).object();
        if (current.value("status").toString() != "committed" ||
            current.value("transaction_id") != d->context.value("transaction_id") ||
            current.value("generation") != d->context.value("generation") ||
            current.value("current") != d->context.value("payload_dir"))
            throw std::runtime_error("update.recovery_required");
        return true;
    } catch (const std::exception &e) {
        d->error = QString::fromUtf8(e.what());
        return false;
    }
#else
    return true;
#endif
}
bool StartupUpdateGate::running() {
#ifdef Q_OS_WIN
    if (!d->root.isEmpty() && !d->recordedRunning) {
        try {
            QFile file(QDir(d->root).filePath("current.json"));
            if (!file.open(QIODevice::ReadOnly))
                throw std::runtime_error("update.recovery_required");
            auto current = QJsonDocument::fromJson(file.read(65536)).object();
            FILETIME created, exit, kernel, user;
            if (!GetProcessTimes(GetCurrentProcess(), &created, &exit, &kernel, &user))
                throw std::runtime_error("update.process_identity");
            QJsonObject receipt{{"transaction_id", current.value("transaction_id")},
                                {"generation", current.value("generation")},
                                {"pid", qint64(GetCurrentProcessId())},
                                {"stamp", QString::number((quint64(created.dwHighDateTime) << 32) |
                                                          created.dwLowDateTime)},
                                {"version", d->version}};
            QSaveFile output(QDir(d->root).filePath("update-state/running-receipt.json"));
            output.setDirectWriteFallback(false);
            auto bytes = QJsonDocument(receipt).toJson(QJsonDocument::Compact);
            if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() ||
                !output.commit())
                throw std::runtime_error("update.file_write");
            d->recordedRunning = true;
            if (restricted()) {
                try {
                    d->send("RUNNING");
                } catch (...) { /* Durable receipt lets the next coordinator recover. */
                }
            }
            return true;
        } catch (const std::exception &e) {
            d->error = QString::fromUtf8(e.what());
            return false;
        }
    }
#endif
    return true;
}
} // namespace daymark

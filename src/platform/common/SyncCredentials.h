#pragma once
#include "domain/Task.h"
#include <QByteArray>
#include <QLibrary>
#include <QString>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#endif

namespace daymark {
class SyncCredentialStore {
  public:
    virtual ~SyncCredentialStore() = default;
    virtual Result<QByteArray> read(const QString &profile) = 0;
    virtual Result<void> write(const QString &profile, const QByteArray &bytes) = 0;
    virtual Result<void> remove(const QString &profile) = 0;
};
// No settings/SQLite/plaintext fallback. Native APIs are resolved at runtime so
// optional sync does not add a linker dependency to offline workspace tests.
class SystemSyncCredentials final : public SyncCredentialStore {
  public:
    Result<QByteArray> read(const QString &profile) override {
#ifdef Q_OS_WIN
        QLibrary lib(QStringLiteral("Advapi32"));
        const auto get = reinterpret_cast<decltype(&CredReadW)>(lib.resolve("CredReadW"));
        const auto release = reinterpret_cast<decltype(&CredFree)>(lib.resolve("CredFree"));
        if (!get || !release)
            return {{}, unavailable()};
        PCREDENTIALW value = nullptr;
        const auto name = target(profile);
        if (!get(reinterpret_cast<LPCWSTR>(name.utf16()), CRED_TYPE_GENERIC, 0, &value)) {
            if (GetLastError() == ERROR_NOT_FOUND)
                return {};
            return {{}, denied()};
        }
        QByteArray bytes(reinterpret_cast<const char *>(value->CredentialBlob),
                         qsizetype(value->CredentialBlobSize));
        release(value);
        return {bytes, {}};
#elif defined(Q_OS_MACOS)
        QLibrary security(QStringLiteral("/System/Library/Frameworks/Security.framework/Security"));
        const auto find =
            reinterpret_cast<Find>(security.resolve("SecKeychainFindGenericPassword"));
        const auto free = reinterpret_cast<Free>(security.resolve("SecKeychainItemFreeContent"));
        if (!find || !free)
            return {{}, unavailable()};
        const auto account = profile.toUtf8();
        quint32 length = 0;
        void *data = nullptr;
        const auto status =
            find(nullptr, quint32(service().size()), service().constData(), quint32(account.size()),
                 account.constData(), &length, &data, nullptr);
        if (status == -25300)
            return {};
        if (status != 0)
            return {{}, denied()};
        QByteArray bytes(static_cast<const char *>(data), qsizetype(length));
        free(nullptr, data);
        return {bytes, {}};
#else
        Q_UNUSED(profile);
        return {{}, unavailable()};
#endif
    }
    Result<void> write(const QString &profile, const QByteArray &bytes) override {
        if (bytes.isEmpty() || bytes.size() > 4096)
            return {denied()};
#ifdef Q_OS_WIN
        QLibrary lib(QStringLiteral("Advapi32"));
        const auto put = reinterpret_cast<decltype(&CredWriteW)>(lib.resolve("CredWriteW"));
        if (!put)
            return {unavailable()};
        const auto name = target(profile);
        CREDENTIALW value{};
        value.Type = CRED_TYPE_GENERIC;
        value.TargetName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(name.utf16()));
        value.CredentialBlobSize = DWORD(bytes.size());
        value.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(bytes.constData()));
        value.Persist = CRED_PERSIST_LOCAL_MACHINE;
        if (!put(&value, 0))
            return {denied()};
        return {};
#elif defined(Q_OS_MACOS)
        QLibrary security(QStringLiteral("/System/Library/Frameworks/Security.framework/Security")),
            core(QStringLiteral(
                "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation"));
        const auto find =
            reinterpret_cast<Find>(security.resolve("SecKeychainFindGenericPassword"));
        const auto add = reinterpret_cast<Add>(security.resolve("SecKeychainAddGenericPassword"));
        const auto modify =
            reinterpret_cast<Modify>(security.resolve("SecKeychainItemModifyAttributesAndData"));
        const auto release = reinterpret_cast<Release>(core.resolve("CFRelease"));
        if (!find || !add || !modify || !release)
            return {unavailable()};
        const auto account = profile.toUtf8();
        void *item = nullptr;
        auto status = find(nullptr, quint32(service().size()), service().constData(),
                           quint32(account.size()), account.constData(), nullptr, nullptr, &item);
        if (status == 0) {
            status = modify(item, nullptr, quint32(bytes.size()), bytes.constData());
            release(item);
        } else if (status == -25300)
            status = add(nullptr, quint32(service().size()), service().constData(),
                         quint32(account.size()), account.constData(), quint32(bytes.size()),
                         bytes.constData(), nullptr);
        if (status != 0)
            return {denied()};
        return {};
#else
        Q_UNUSED(profile);
        return {unavailable()};
#endif
    }
    Result<void> remove(const QString &profile) override {
#ifdef Q_OS_WIN
        QLibrary lib(QStringLiteral("Advapi32"));
        const auto erase = reinterpret_cast<decltype(&CredDeleteW)>(lib.resolve("CredDeleteW"));
        if (!erase)
            return {unavailable()};
        const auto name = target(profile);
        if (!erase(reinterpret_cast<LPCWSTR>(name.utf16()), CRED_TYPE_GENERIC, 0) &&
            GetLastError() != ERROR_NOT_FOUND)
            return {denied()};
        return {};
#elif defined(Q_OS_MACOS)
        QLibrary security(QStringLiteral("/System/Library/Frameworks/Security.framework/Security")),
            core(QStringLiteral(
                "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation"));
        const auto find =
            reinterpret_cast<Find>(security.resolve("SecKeychainFindGenericPassword"));
        const auto erase = reinterpret_cast<Erase>(security.resolve("SecKeychainItemDelete"));
        const auto release = reinterpret_cast<Release>(core.resolve("CFRelease"));
        if (!find || !erase || !release)
            return {unavailable()};
        const auto account = profile.toUtf8();
        void *item = nullptr;
        const auto status =
            find(nullptr, quint32(service().size()), service().constData(), quint32(account.size()),
                 account.constData(), nullptr, nullptr, &item);
        if (status == -25300)
            return {};
        if (status != 0)
            return {denied()};
        const auto result = erase(item);
        release(item);
        return result == 0 ? Result<void>{} : Result<void>{denied()};
#else
        Q_UNUSED(profile);
        return {unavailable()};
#endif
    }

  private:
    static QString target(const QString &p) {
        return QStringLiteral("Daymark.Sync.v1/") + p;
    }
    static Error unavailable() {
        return {ErrorCode::UnsupportedPlatform, QStringLiteral("syncCredentialsUnavailable")};
    }
    static Error denied() {
        return {ErrorCode::PermissionError, QStringLiteral("syncCredentialsLocked")};
    }
#ifdef Q_OS_MACOS
    static QByteArray service() {
        return QByteArrayLiteral("Daymark.Sync.v1");
    }
    using Find = qint32 (*)(void *, quint32, const char *, quint32, const char *, quint32 *,
                            void **, void **);
    using Free = qint32 (*)(void *, void *);
    using Add = qint32 (*)(void *, quint32, const char *, quint32, const char *, quint32,
                           const void *, void **);
    using Modify = qint32 (*)(void *, const void *, quint32, const void *);
    using Erase = qint32 (*)(void *);
    using Release = void (*)(const void *);
#endif
};
} // namespace daymark

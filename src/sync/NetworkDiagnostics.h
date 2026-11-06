#pragma once
#include "diagnostics/RuntimeLog.h"
#include <QJsonArray>
#include <QMetaEnum>
#include <QNetworkReply>
#include <QSslError>
#include <QUrl>

namespace daymark::sync {
inline QJsonObject networkEvidence(QNetworkReply::NetworkError error, QString description,
                                   const QList<QSslError> &tlsErrors, const QString &origin,
                                   int httpStatus, int budgetMs, const QStringList &secrets = {}) {
    const auto enumerator = QMetaEnum::fromType<QNetworkReply::NetworkError>();
    const auto key = enumerator.valueToKey(static_cast<int>(error));
    const auto name = key ? QString::fromLatin1(key) : QStringLiteral("UnknownNetworkError");
    // HTTP error descriptions may contain a server-controlled reason phrase.
    // Preserve transport diagnostics, never a response body or echoed payload.
    if (httpStatus > 0)
        description = name;
    for (const auto &secret : secrets)
        if (!secret.isEmpty())
            description.replace(secret, "<secret>");
    const auto host = QUrl(origin).host();
    if (!host.isEmpty())
        description.replace(host, "<host>", Qt::CaseInsensitive);
    QJsonArray tls;
    const auto tlsEnumerator = QMetaEnum::fromType<QSslError::SslError>();
    for (const auto &issue : tlsErrors.mid(0, 32)) {
        const auto symbol = tlsEnumerator.valueToKey(static_cast<int>(issue.error()));
        tls.append(QJsonObject{
            {"code", static_cast<int>(issue.error())},
            {"name", symbol ? QString::fromLatin1(symbol) : QStringLiteral("UnspecifiedError")}});
    }
    return {{"qt_network_error", static_cast<int>(error)},
            {"qt_network_error_name", name},
            {"network_error_detail", diagnostics::redact(description)},
            {"tls_errors", tls},
            {"inactivity_timeout_ms", budgetMs},
            {"native_error_available", false},
            {"native_error_source", QStringLiteral("not_exposed_by_qt_network_reply")}};
}
} // namespace daymark::sync

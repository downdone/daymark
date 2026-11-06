#pragma once
#include <QJsonObject>
#include <QString>

namespace daymark::diagnostics {
struct LogPolicy {
    qint64 maximumFileBytes = 2 * 1024 * 1024;
    int retainedFiles = 10;
    int retainedDays = 7;
};
bool start(const QString &directory, LogPolicy policy = {});
void stop();
QString directory();
QString error();
QString redact(QString text);
void event(const QString &component, const QString &name, const QJsonObject &fields = {},
           const QString &level = QStringLiteral("info"));
} // namespace daymark::diagnostics

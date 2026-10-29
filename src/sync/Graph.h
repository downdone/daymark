#pragma once
#include "Protocol.h"
#include <QMap>
namespace daymark::sync {
struct Graph {
    QMap<QString, QJsonObject> entities;
    QHash<QString, qint64> localVersions;
    QJsonArray aliases, edges;
    QHash<QString, QString> taskWireIds, taskLocalIds, scopeWireIds;
    QHash<QString, QString> ruleCutoffs;
    QJsonArray array() const {
        QJsonArray a;
        for (const auto &e : entities)
            a.append(e);
        return a;
    }
};
inline QString key(const QJsonObject &e) {
    return e.value("type").toString() + QLatin1Char(':') + e.value("id").toString();
}
} // namespace daymark::sync

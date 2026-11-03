#pragma once
#include "services/TaskService.h"
#include <QCryptographicHash>
#include <QVariantList>
#include <QVariantMap>
#include <QHash>

namespace daymark {
// Local presentation state only. Delivery/recurrence facts remain in SQLite.
struct ReminderPresentation {
    static QString key(const QString& taskId,const QString& occurrence) {
        return QString::fromLatin1(QCryptographicHash::hash((taskId+QLatin1Char('\n')+occurrence).toUtf8(),QCryptographicHash::Sha256).toHex());
    }
    static QVariantList pending(const QVariantList& candidates,const QHash<QString,Task>& tasks,const QVariantMap& state,const QDateTime& now) {
        QVariantList result;
        for(const auto& value:candidates) {
            auto alert=value.toMap();const auto id=alert.value("taskId").toString();
            const auto task=tasks.constFind(id);if(task==tasks.cend())continue;
            if(!QueryService::effectiveActive(*task) || task->remindAtUtc.isEmpty() || task->reminderFiredAtUtc!=alert.value("firedAt").toString())continue;
            const auto identity=key(id,alert.value("occurrenceAt").toString());const auto saved=state.value(identity).toMap();
            if(saved.value("dismissed").toBool())continue;
            const auto until=QDateTime::fromString(saved.value("until").toString(),Qt::ISODateWithMs);
            const auto fired=QDateTime::fromString(alert.value("firedAt").toString(),Qt::ISODateWithMs);
            if(until.isValid() && until>now)continue;
            if(!fired.isValid() || (fired<now.addDays(-1) && (!until.isValid() || until<now.addDays(-1))))continue;
            alert.insert("key",identity);alert.insert("title",task->title);alert.insert("rowVersion",task->rowVersion);
            result.append(alert);
        }
        return result;
    }
};
}

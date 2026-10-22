#pragma once
#include "domain/Task.h"
#include <QSqlDatabase>
namespace daymark {
class RecurrenceRepository final {
public:
    explicit RecurrenceRepository(QSqlDatabase db):m_db(std::move(db)){}
    Result<QList<RecurrenceRule>> all() const;
    Result<RecurrenceRule> save(RecurrenceRule rule,const QString& now,const QDate& today);
    Result<void> stop(const QString& id,qint64 version,const QString& now,bool removeFuture,const QDate& today);
    Result<int> materialize(const RecurrenceRule& rule,const QList<QDate>& dates,const QString& now,const QDate& occurrenceKey={});
private:
    QSqlDatabase m_db;
};
}

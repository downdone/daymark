#pragma once
#include "persistence/RecurrenceRepository.h"
#include "persistence/TaskRepository.h"
namespace daymark {
class RecurrenceService final {
public:
    RecurrenceService(RecurrenceRepository& rules,TaskRepository& tasks,const IClock& clock):m_rules(rules),m_tasks(tasks),m_clock(clock){}
    static QList<QDate> nextDates(const RecurrenceRule& rule,const QDate& from,int limit=3);
    static bool occursOn(const RecurrenceRule& rule,const QDate& date);
    Result<RecurrenceRule> save(RecurrenceRule rule);
    Result<void> stop(const QString& id,qint64 version,bool removeFuture);
    Result<int> generate(const QDate& from={},const QDate& through={},const QStringList& completionPriority={});
private:
    RecurrenceRepository& m_rules;
    TaskRepository& m_tasks;
    const IClock& m_clock;
};
}

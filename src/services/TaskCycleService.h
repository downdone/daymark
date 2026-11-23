#pragma once

#include "persistence/TaskCycleRepository.h"
#include "persistence/TaskRepository.h"

namespace daymark {
class TaskCycleService final {
public:
    TaskCycleService(TaskCycleRepository& cycles,TaskRepository& tasks,const IClock& clock)
        :m_cycles(cycles),m_tasks(tasks),m_clock(clock){}

    static QString occurrenceKey(const QString& taskId,const TaskCycle& cycle);
    static Result<TaskCycle> first(const Task& task,RecurrenceRule rule,const QString& zone,const QDate& today);
    static Result<QPair<QDate,QDateTime>> next(const Task& task,const TaskCycle& cycle,const QDateTime& after);
    Result<void> configure(const Task& task,const TaskCycle& proposed);
    Result<void> stop(const Task& task,qint64 version);
    Result<void> convertLegacy(const Task& task,qint64 version);
    Result<void> complete(const Task& before,const Task& after,const QStringList& members,bool skipped=false);
    Result<void> undo(const Task& before);
    Result<int> advance(int limit=32);

private:
    TaskCycleRepository& m_cycles;
    TaskRepository& m_tasks;
    const IClock& m_clock;
};
}

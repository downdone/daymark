#pragma once
#include "TaskService.h"

namespace daymark {
struct CalendarEntry {
    Task task;
    QString kind; // start, deadline, both; presentation never infers date semantics.
};
struct CalendarDay {
    QDate date;
    bool inMonth=false;
    QList<CalendarEntry> entries;
};
class CalendarService final {
public:
    static QList<CalendarEntry> entries(const QList<Task>& tasks,const QDate& date);
    static QList<CalendarDay> days(const QList<Task>& tasks,const QDate& anchor,bool week,bool sundayFirst);
    static Result<Task> move(TaskService& service,const QString& id,qint64 version,const QString& kind,const QDate& date);
};
}

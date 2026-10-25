#pragma once
#include "TaskService.h"
#include <QTimeZone>

namespace daymark {
struct CalendarEntry {
    Task task;
    QString kind; // start, deadline, both, range; presentation never infers date semantics.
};
struct CalendarDay {
    QDate date;
    bool inMonth=false;
    QList<CalendarEntry> entries;
};
struct CalendarTaskRange {Task task;QDate start,end;};
struct CalendarTimeBlock {Task task;QDate date;double startMinute=0,endMinute=0;int lane=0,laneCount=1;bool continuesBefore=false,continuesAfter=false;int cluster=0;};
class CalendarService final {
public:
    static QList<CalendarTimeBlock> timeBlocks(const QList<Task>& tasks,const QDate& from,int days,const QTimeZone& zone,bool includeCompleted=false);
    static Result<Task> moveTime(TaskService& service,const Task& before,qint64 version,const QDate& date,int minute,const QString& edge,const QTimeZone& zone);
    static Result<Task> moveDate(TaskService& service,const Task& before,qint64 version,const QDate& from,const QDate& date,const QString& edge,const QTimeZone& zone);
    static QString timeSummary(const Task& task,const QDate& today,const QTimeZone& zone);
    static QList<CalendarTaskRange> ranges(const QList<Task>& tasks,const QDate& from,const QDate& to,bool includeCompleted=false);
    static QList<CalendarEntry> entries(const QList<Task>& tasks,const QDate& date,bool includeCompleted=false);
    static QList<CalendarDay> days(const QList<Task>& tasks,const QDate& anchor,bool week,bool sundayFirst,bool includeCompleted=false);
    static QList<Task> unscheduled(const QList<Task>& tasks);
    static Result<Task> move(TaskService& service,const QString& id,qint64 version,const QString& kind,const QDate& date);
};
}

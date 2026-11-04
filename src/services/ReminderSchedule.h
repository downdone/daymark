#pragma once
#include "domain/Task.h"
#include <QDateTime>
#include <QTimeZone>
namespace daymark {
class ReminderSchedule {
public:
    static QDateTime nextPending(const QList<Task>& tasks);
    static QDateTime nextAfter(const QDateTime& anchor,const QString& frequency,int interval,const QTimeZone& zone,const QDateTime& now);
};
}

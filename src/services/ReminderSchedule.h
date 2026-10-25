#pragma once
#include <QDateTime>
#include <QTimeZone>
namespace daymark {
class ReminderSchedule {
public:
    static QDateTime nextAfter(const QDateTime& anchor,const QString& frequency,int interval,const QTimeZone& zone,const QDateTime& now);
};
}

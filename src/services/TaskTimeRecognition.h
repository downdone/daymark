#pragma once
#include "domain/Task.h"
#include <QTimeZone>

namespace daymark {
struct TimeRecognitionOptions {
    bool enabled=true;
    bool dateOnlyReminder=false;
    QTime dateOnlyTime{9,0};
};
struct RecognizedTaskTime {
    enum class State { None, Ready, Choice };
    State state=State::None;
    QString reason;
    QString target=QStringLiteral("planned");
    QDate date;
    QTime time;
    QDateTime reminderUtc;
    QList<QPair<int,int>> spans; // UTF16 start/length, never persisted.
};
struct TaskTimeOverride {
    QDate date;
    QTime time;
    QString target=QStringLiteral("planned");
    bool reminder=false;
};
class TaskTimeRecognition final {
public:
    static RecognizedTaskTime parse(const QString&,const QDateTime& nowUtc,const QTimeZone&,const TimeRecognitionOptions& = {});
    static Result<RecognizedTaskTime> manual(const TaskTimeOverride&,const QDateTime& nowUtc,const QTimeZone&);
};
}

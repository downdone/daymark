#pragma once
#include "Task.h"
#include <QTimeZone>
namespace daymark {
struct EventLink {QString taskId,role=QStringLiteral("preparation");};
struct CalendarEvent {
    QString id,title,notes,timeZone;
    bool allDay=true;
    QDate startOn,endOn;
    QDateTime startUtc,endUtc;
    qint64 rowVersion=1;
    QList<EventLink> links;
};
}

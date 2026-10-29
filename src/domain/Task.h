#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QList>
#include <optional>

namespace daymark {

enum class ErrorCode { None, ValidationError, NotFound, Conflict, PersistenceError, PermissionError, UnsupportedPlatform, Cancelled };
struct Error { ErrorCode code = ErrorCode::None; QString key; };

template<class T> struct Result {
    T value{};
    Error error;
    bool ok() const { return error.code == ErrorCode::None; }
    static Result failure(ErrorCode code, const QString& key) { return {T{}, {code, key}}; }
};
template<> struct Result<void> {
    Error error;
    bool ok() const { return error.code == ErrorCode::None; }
    static Result failure(ErrorCode code, const QString& key) { return {{code, key}}; }
};

template<class T> struct FieldPatch {
    enum class Kind { Unspecified, Clear, Set };
    Kind kind = Kind::Unspecified;
    T value{};
    static FieldPatch set(T value) { return {Kind::Set, std::move(value)}; }
    static FieldPatch clear() { return {Kind::Clear, {}}; }
};

class IClock {
public:
    virtual ~IClock() = default;
    virtual QDate localToday() const = 0;
    virtual QDateTime nowUtc() const = 0;
};
class SystemClock final : public IClock {
public:
    QDate localToday() const override { return QDate::currentDate(); }
    QDateTime nowUtc() const override { return QDateTime::currentDateTimeUtc(); }
};

struct CatalogItem {
    QString id,kind,title,parentId,planningState=QStringLiteral("ACTIVE"),status=QStringLiteral("OPEN");
    qint64 rowVersion=1;
    QString description,colorKey=QStringLiteral("neutral");
    std::optional<QDate> startOn,deadlineOn;
    bool archived=false;
};
struct RecurrenceRule {
    QString id,title,notes,frequency=QStringLiteral("DAILY");
    QDate anchor,end;
    int interval=1,weekdays=0,monthDay=1,yearMonth=1;
    bool active=true;
    QString sourceTaskId,defaultsJson=QStringLiteral("{}");
    qint64 rowVersion=1;
    bool operator==(const RecurrenceRule&) const = default;
};
struct TaskSchedule {
    bool allDay=true;
    QDate startOn,endOn;
    QDateTime startUtc,endUtc;
    QString timeZone;
    int alertMinutes=-1;
    bool operator==(const TaskSchedule&) const = default;
};
struct TaskLink {
    QString taskId,role=QStringLiteral("preparation");
    bool operator==(const TaskLink&) const = default;
};

struct Task {
    QString id, title, notes;
    QString parentTaskId,parentTitle;
    std::optional<TaskSchedule> schedule;
    std::optional<RecurrenceRule> repeat;
    QList<TaskLink> links;
    // Command-only context; never stored as another task fact.
    bool repeatChanged=false;
    QString replacedRecurrenceId;
    qint64 replacedRecurrenceVersion=0;
    QDate repeatCutoff;
    bool pinned=false,important=false;
    int listDepth=0,listChildCount=0,totalSubtasks=0;
    bool parentDeleted=false;
    QString planningState = QStringLiteral("INBOX");
    QString status = QStringLiteral("OPEN");
    std::optional<QDate> startOn, deadlineOn;
    bool todayFocus = false;
    bool evening = false;
    QString createdAtUtc, updatedAtUtc, completedAtUtc, cancelledAtUtc, deletedAtUtc;
    QString deletionGroupId,completionGroupId;
    qint64 rowVersion = 1;
    qint64 sortOrder = 0;
    QString projectId, projectPlanning, projectStatus,areaId,headingId,projectName,projectAreaId,headingTitle;
    QString areaName,categoryOrderPath;
    bool areaArchived=false;
    qint64 headingOrder=-1;
    QStringList tagIds,tagNames;
    int pendingSubtasks=0;
    QString remindAtUtc,reminderFiredAtUtc;
    QString reminderRepeat=QStringLiteral("NONE"),reminderAnchorUtc,reminderZone=QStringLiteral("UTC");
    int reminderEvery=1;
    QString recurrenceId;
    QString syncOccurrenceKey;
    QDate syncOccurrenceOn;
    QDate occurrenceOn;
    bool instanceOverridden=false;
    qint64 todayOrder=-1;
    bool projectDeleted = false;
    // Read projection only: preserve the editable conflict candidate on disk,
    // while a server deletion remains hidden until the user resolves it.
    bool syncRemoteDeletionPending = false;
    bool operator==(const Task&) const = default;
};

struct TaskPatch {
    FieldPatch<qint64> repeatVersion;
    FieldPatch<TaskSchedule> schedule;
    FieldPatch<RecurrenceRule> repeat;
    FieldPatch<QList<TaskLink>> links;
    FieldPatch<QString> title, notes, planningState, parentTaskId;
    FieldPatch<bool> pinned,important;
    FieldPatch<QDate> startOn, deadlineOn;
    FieldPatch<bool> todayFocus;
    FieldPatch<bool> evening;
    FieldPatch<QString> projectId,areaId,headingId,remindAtUtc,reminderRepeat,reminderZone;
    FieldPatch<int> reminderEvery;
    FieldPatch<QStringList> tagIds;
};
struct OrderChange {
    QString taskId;
    std::optional<bool> evening;
    std::optional<QString> headingId;
};

} // namespace daymark

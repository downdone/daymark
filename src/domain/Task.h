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

struct ChecklistItem {
    QString id,title;
    bool done=false;
    qint64 sortOrder=0;
    bool operator==(const ChecklistItem&) const = default;
};
struct CatalogItem {
    QString id,kind,title,parentId,planningState=QStringLiteral("ACTIVE"),status=QStringLiteral("OPEN");
    qint64 rowVersion=1;
    QString description,colorKey=QStringLiteral("neutral");
    std::optional<QDate> startOn,deadlineOn;
};
struct RecurrenceRule {
    QString id,title,notes,frequency=QStringLiteral("DAILY");
    QDate anchor,end;
    int interval=1,weekdays=0,monthDay=1,yearMonth=1;
    bool active=true;
    qint64 rowVersion=1;
};
struct Task {
    QString id, title, notes;
    QString planningState = QStringLiteral("INBOX");
    QString status = QStringLiteral("OPEN");
    std::optional<QDate> startOn, deadlineOn;
    bool todayFocus = false;
    bool evening = false;
    QString createdAtUtc, updatedAtUtc, completedAtUtc, cancelledAtUtc, deletedAtUtc;
    qint64 rowVersion = 1;
    qint64 sortOrder = 0;
    QString projectId, projectPlanning, projectStatus,areaId,headingId,projectName,projectAreaId,headingTitle;
    qint64 headingOrder=-1;
    QStringList tagIds,tagNames;
    QList<ChecklistItem> checklist;
    QString remindAtUtc,reminderFiredAtUtc;
    QString recurrenceId;
    QDate occurrenceOn;
    bool instanceOverridden=false;
    qint64 todayOrder=-1;
    bool projectDeleted = false;
    bool operator==(const Task&) const = default;
};

struct TaskPatch {
    FieldPatch<QString> title, notes, planningState;
    FieldPatch<QDate> startOn, deadlineOn;
    FieldPatch<bool> todayFocus;
    FieldPatch<bool> evening;
    FieldPatch<QString> projectId,areaId,headingId,remindAtUtc;
    FieldPatch<QStringList> tagIds;
    FieldPatch<QList<ChecklistItem>> checklist;
};
struct OrderChange {
    QString taskId;
    std::optional<bool> evening;
    std::optional<QString> headingId;
};

} // namespace daymark

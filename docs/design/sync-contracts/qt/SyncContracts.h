#pragma once
// Design-only C++20/Qt 6.10.2 contracts; no QObject, signals, network or SQL code.
#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVector>
#include <QUuid>
#include <QUrl>
#include <functional>
#include <optional>
#include <variant>

namespace daymark::sync::design {
struct ProfileId { QUuid value; };
struct MutationId { QUuid value; };
struct RequestId { QUuid value; };
struct EntityId { QUuid value; };
struct Generation { qint64 value; };
struct ServerRevision { qint64 value; };
struct LocalRowVersion { qint64 value; };
struct LocalSeq { qint64 value; };
struct ServerSeq { qint64 value; };
struct Digest { QByteArray bytes; }; // Exactly 32 bytes, verified at codec boundary.
enum class FieldKind { Unspecified, Clear, Set };
template<class T> struct Field { FieldKind kind{FieldKind::Unspecified}; std::optional<T> value; };
struct Error { QString code; QString fieldPath; bool retryable{}; std::optional<int> retryAfterMs; };
template<class T> using Result = std::variant<T, Error>;
using Done = Result<std::monostate>;
struct RequestContext {
    ProfileId profile; Generation generation; RequestId request;
    std::optional<MutationId> mutation;
};
struct EntityKey { QString type; EntityId id; };
struct EntityVersion { EntityKey key; ServerRevision revision; };
struct StructureVersion { QString kind; QUuid id; ServerRevision revision; };
struct AllDaySchedule { QDate startOn, endOn; }; // End exclusive.
struct TimedSchedule { QDateTime startUtc, endUtc; QByteArray timeZone; };
using Schedule = std::variant<AllDaySchedule, TimedSchedule>;
enum class Repeat { None, Daily, Weekly, Monthly, Yearly };
struct AbsoluteReminder { QDateTime anchorUtc; Repeat repeat; int every; QByteArray timeZone; };
struct RelativeReminder { int minutesBefore; QByteArray timeZone; };
using Reminder = std::variant<AbsoluteReminder, RelativeReminder>;
struct Link { EntityId target; QString role; };
struct TaskPatch {
    Field<QString> title, notes, planningState;
    Field<EntityId> categoryId, parentTaskId;
    Field<QDate> startOn, deadlineOn;
    Field<bool> todayFocus, evening, pinned, important;
    Field<Schedule> schedule; Field<Reminder> reminder;
    Field<QVector<EntityId>> tagIds; Field<QVector<Link>> links;
};
// Full command union is normatively defined by OpenAPI Command and Go commands.go.
// Qt codec must decode every command variant before capture; no generic upsert.
struct CapturedCommand {
    QString type; QByteArray validatedCommandJson; QDateTime eventAtUtc;
};
struct CapturedWrite { EntityKey key; LocalRowVersion afterVersion; };
struct JournalEntry { MutationId mutation; LocalSeq sequence; QVector<CapturedCommand> commands; QVector<CapturedWrite> writes; };
struct FrozenMutation {
    MutationId mutation; LocalSeq sequence; QUuid originEpoch;
    QByteArray immutablePayload; Digest canonicalHash;
};
struct Receipt {
    MutationId mutation; QUuid dataEpoch; QString result;
    std::optional<ServerSeq> batchSeq; ServerSeq observedHead;
    QVector<EntityVersion> entities; QVector<StructureVersion> structures;
};
// Factory/codec validates exact schema, complete batch and hashes. QByteArray is
// immutable serialized DTO boundary, never exposed to QML or reused as Task model.
struct VerifiedBatch { ServerSeq sequence; QUuid dataEpoch; QByteArray fullBatch; Digest hash; QByteArray afterCursor; };
struct PullPage { QVector<VerifiedBatch> batches; QByteArray nextCursor; bool hasMore; ServerSeq through; };
struct ChangeSet { QVector<EntityKey> changed; int pendingCount{}, conflictCount{}; };
struct Binding {
    ProfileId profile; QUrl serverOrigin; QUuid serverInstance, user, dataset, dataEpoch;
};
template<class T> using Completion = std::function<void(RequestContext, Result<T>)>;
class ISyncTransport {
public:
    virtual ~ISyncTransport() = default;
    virtual void push(RequestContext, FrozenMutation, Completion<Receipt>) = 0;
    virtual void receipt(RequestContext, MutationId, QUuid epoch, Completion<std::optional<Receipt>>) = 0;
    virtual void pull(RequestContext, QByteArray cursor, Completion<PullPage>) = 0;
    virtual void cancel(RequestId) = 0; // Does not mean server commit was cancelled.
};
class ISyncRepository {
public:
    virtual ~ISyncRepository() = default;
    // DB-worker only; caller cannot carry Tx/Rows across a queued callback.
    virtual Result<std::optional<FrozenMutation>> prepareNext(ProfileId, Generation, QDateTime) = 0;
    virtual Done markInFlight(ProfileId, Generation, MutationId, Digest) = 0;
    virtual Result<ChangeSet> acknowledge(ProfileId, Generation, Receipt) = 0;
    virtual Result<ChangeSet> applyBatch(ProfileId, Generation, QByteArray expectedCursor,
                                        VerifiedBatch) = 0;
    virtual Result<MutationId> resolveConflict(ProfileId, Generation, QUuid conflictId,
                                               QByteArray validatedDecision, ServerRevision) = 0;
};
struct CredentialKey { QString origin; QUuid user, session; };
struct TokenPair { QByteArray access, refresh; QDateTime accessExpiry, refreshExpiry; qint64 generation; };
class ICredentialStore {
public:
    virtual ~ICredentialStore() = default;
    virtual Result<std::optional<TokenPair>> read(CredentialKey) = 0;
    virtual Done writePairAtomically(CredentialKey, TokenPair) = 0;
    virtual Done removePair(CredentialKey) = 0;
};
} // namespace daymark::sync::design

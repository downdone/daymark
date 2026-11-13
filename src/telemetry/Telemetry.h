#pragma once
#include "domain/Task.h"
#include <QDateTime>
#include <QJsonObject>
#include <QString>

namespace daymark::telemetry {
enum class Surface { Main, Widget, Mini, Ball };
enum class UpdateOutcome { Ready, Updated, Cancelled, Failed };
// No arbitrary map/string event API is exposed to QML or task models.
struct Event {
    enum class Kind { Startup, SurfaceUsed, UpdateResult, Performance } kind;
    qint64 measurement=0;
    Surface surface=Surface::Main;
    UpdateOutcome outcome=UpdateOutcome::Ready;
    static Event startup(qint64 ms) {return {Kind::Startup,ms};}
    static Event surfaceUsed(Surface value) {Event e{Kind::SurfaceUsed};e.surface=value;return e;}
    static Event updateResult(UpdateOutcome value) {Event e{Kind::UpdateResult};e.outcome=value;return e;}
    static Event performance(qint64 bytes) {return {Kind::Performance,bytes};}
    QJsonObject payload() const;
};

// Called only on the existing workspace worker. Each call owns a short-lived
// independent connection; never opens/migrates the task database.
class Queue final {
public:
    static QString path(const QString& taskDatabase,const QString& binding);
    static Result<QJsonObject> execute(const QString& file,const QString& binding,
        const QString& operation,const QJsonObject& input={},qint64 now=QDateTime::currentMSecsSinceEpoch());
};
}

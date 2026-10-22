#pragma once
#include "SqliteDatabase.h"
#include <QJsonObject>
namespace daymark {
struct ImportPlan {QJsonObject document;int rows=0;};
struct ImportOutcome {int added=0,skipped=0;bool settingsApplied=true;};
class DataTransfer final {
public:
    explicit DataTransfer(SqliteDatabase& db):m_database(db){}
    Result<void> exportJson(const QString& path) const;
    Result<void> exportCsv(const QString& path) const;
    static Result<ImportPlan> preview(const QString& path,bool snapshot=false);
    Result<ImportOutcome> apply(const ImportPlan& plan,bool replace);
private:
    static Result<QJsonObject> document(QSqlDatabase db);
    static Result<ImportOutcome> insert(QSqlDatabase db,const QJsonObject& document,bool replace,bool validateOnly=false);
    SqliteDatabase& m_database;
};
}

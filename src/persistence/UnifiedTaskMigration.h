#pragma once
#include "domain/Task.h"
#include <QSqlDatabase>
namespace daymark {
// Runs inside the caller's migration/import transaction. Never opens a user DB.
Result<void> convertLegacyEvents(QSqlDatabase db,const QDateTime& now);
}

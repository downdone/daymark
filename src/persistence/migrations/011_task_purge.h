#pragma once
#include <QStringList>
namespace daymark {
// Append-only migration. Local suppression never changes server facts.
inline QStringList taskPurgeMigration11() {
    return {QStringLiteral("ALTER TABLE tasks ADD COLUMN local_removed_at_utc TEXT"),
            QStringLiteral("ALTER TABLE tasks ADD COLUMN purged_at_utc TEXT CHECK(purged_at_utc IS NULL OR (deleted_at_utc IS NOT NULL AND title='[deleted]' AND note_text=''))")};
}
}

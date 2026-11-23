#pragma once

#include <QStringList>

namespace daymark {
inline QStringList taskCyclesMigration12() {
    return {
        QStringLiteral("CREATE TABLE task_cycles("
                       "task_id TEXT PRIMARY KEY NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,"
                       "frequency TEXT NOT NULL CHECK(frequency IN ('DAILY','WEEKLY','MONTHLY','YEARLY','AFTER_DAYS')),"
                       "interval_n INTEGER NOT NULL CHECK(interval_n BETWEEN 1 AND 36500),"
                       "anchor_on TEXT NOT NULL,weekdays_mask INTEGER NOT NULL CHECK(weekdays_mask BETWEEN 0 AND 127),"
                       "month_day INTEGER NOT NULL CHECK(month_day BETWEEN 1 AND 31),"
                       "year_month INTEGER NOT NULL CHECK(year_month BETWEEN 1 AND 12),until_on TEXT,"
                       "zone TEXT NOT NULL,active INTEGER NOT NULL CHECK(active IN (0,1)),"
                       "current_on TEXT NOT NULL,current_start_utc TEXT NOT NULL,current_end_utc TEXT,"
                       "next_on TEXT,next_start_utc TEXT,rule_revision INTEGER NOT NULL CHECK(rule_revision>0),"
                       "row_version INTEGER NOT NULL CHECK(row_version>0),"
                       "CHECK((next_on IS NULL)=(next_start_utc IS NULL)))"),
        QStringLiteral("CREATE INDEX task_cycle_due ON task_cycles(next_start_utc,task_id) WHERE active=1 AND next_start_utc IS NOT NULL"),
        QStringLiteral("CREATE TABLE task_cycle_completions("
                       "task_id TEXT NOT NULL REFERENCES task_cycles(task_id) ON DELETE CASCADE,"
                       "occurrence_key TEXT NOT NULL,rule_revision INTEGER NOT NULL,"
                       "scheduled_start_utc TEXT NOT NULL,scheduled_end_utc TEXT,"
                       "outcome TEXT NOT NULL CHECK(outcome IN ('COMPLETED','SKIPPED')),"
                       "performed_at_utc TEXT NOT NULL,mutation_id TEXT UNIQUE NOT NULL,"
                       "completion_group_id TEXT,members_json TEXT NOT NULL DEFAULT '[]',revoked_at_utc TEXT,"
                       "prior_reminder_utc TEXT,prior_reminder_fired_utc TEXT,"
                       "PRIMARY KEY(task_id,occurrence_key))"),
        QStringLiteral("CREATE INDEX task_cycle_history ON task_cycle_completions(task_id,performed_at_utc DESC,occurrence_key)")
    };
}
}

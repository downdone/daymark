#pragma once
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

namespace daymark {
// A savepoint is a transaction when outermost, and composes safely with a
// surrounding command/journal transaction. Destruction never commits.
class ScopedTransaction final {
  public:
    explicit ScopedTransaction(QSqlDatabase db)
        : m_db(std::move(db)),
          m_name(QStringLiteral("dm_") + QUuid::createUuid().toString(QUuid::Id128)) {
        QSqlQuery q(m_db);
        m_active = q.exec(QStringLiteral("SAVEPOINT ") + m_name);
    }
    ~ScopedTransaction() {
        rollback();
    }
    ScopedTransaction(const ScopedTransaction &) = delete;
    ScopedTransaction &operator=(const ScopedTransaction &) = delete;
    bool active() const {
        return m_active;
    }
    bool commit() {
        if (!m_active)
            return false;
        QSqlQuery q(m_db);
        if (!q.exec(QStringLiteral("RELEASE ") + m_name))
            return false;
        m_active = false;
        return true;
    }
    bool rollback() {
        if (!m_active)
            return true;
        QSqlQuery q(m_db);
        const bool first = q.exec(QStringLiteral("ROLLBACK TO ") + m_name);
        const bool second = q.exec(QStringLiteral("RELEASE ") + m_name);
        m_active = false;
        return first && second;
    }

  private:
    QSqlDatabase m_db;
    QString m_name;
    bool m_active = false;
};
} // namespace daymark

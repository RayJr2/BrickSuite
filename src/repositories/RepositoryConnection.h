#pragma once

#include "../database/DatabaseManager.h"

#include <QSqlDatabase>
#include <QThread>

// Connection ownership adapter for the small repository slice used by Host reads.
// It stores only the connection name; a QSqlDatabase handle is obtained and used
// on the thread that constructed the repository.
class RepositoryConnection
{
protected:
    RepositoryConnection()
        : RepositoryConnection(DatabaseManager::instance().database()) {}
    explicit RepositoryConnection(const QSqlDatabase& database)
        : m_connectionName(database.connectionName())
        , m_ownerThread(QThread::currentThread())
    {
        Q_ASSERT(database.isValid());
        Q_ASSERT(!m_connectionName.isEmpty());
    }

    QSqlDatabase repositoryDatabase() const
    {
        Q_ASSERT(QThread::currentThread() == m_ownerThread);
        QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
        Q_ASSERT(database.isValid());
        return database;
    }

private:
    QString m_connectionName;
    QThread* m_ownerThread = nullptr;
};

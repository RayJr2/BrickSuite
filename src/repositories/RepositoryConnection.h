/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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

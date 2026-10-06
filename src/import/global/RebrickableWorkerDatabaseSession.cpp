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

#include "RebrickableWorkerDatabaseSession.h"

#include <QDebug>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

bool RebrickableWorkerDatabaseSession::execute(const QString& databasePath,
                                               const Work& work,
                                               QString& errorMessage)
{
    if (QCoreApplication::instance()
        && QThread::currentThread() == QCoreApplication::instance()->thread()) {
        errorMessage = QStringLiteral("The Rebrickable worker database session must run on a worker thread.");
        return false;
    }
    const QString connectionName = QStringLiteral("rebrickable-import-%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool success = false;
    qInfo() << "Rebrickable import worker database session started.";
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!database.open()) {
            errorMessage = QStringLiteral("Unable to open the worker database: %1")
                               .arg(database.lastError().text());
        } else {
            QSqlQuery pragma(database);
            if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
                errorMessage = QStringLiteral("Unable to enable worker foreign keys: %1")
                                   .arg(pragma.lastError().text());
            } else if (!pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"))) {
                errorMessage = QStringLiteral("Unable to configure worker busy timeout: %1")
                                   .arg(pragma.lastError().text());
            } else {
                success = work(database, errorMessage);
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    qInfo() << "Rebrickable import worker database session stopped. Success:" << success;
    return success;
}

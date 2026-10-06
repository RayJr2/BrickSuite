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

#include <QList>
#include <QSqlDatabase>
#include <QStringList>

class CatalogSetPartOutService
{
public:
    static constexpr int MaximumCopies = 1000000;
    struct Request {
        int workspaceId = 0, setCatalogId = 0, copies = 1;
        bool includeSpares = true;
        QString condition = QStringLiteral("Used");
        bool createStorage = true;
        int storageId = 0, storageTypeId = 0, parentStorageId = 0;
        QString storageName;
        QString operationId;
    };
    struct Row {
        int partId = 0, colorId = 0;
        QString partNumber, partName, colorName;
        qint64 perSet = 0, total = 0;
        bool spare = false;
    };
    struct Plan {
        bool success = false;
        QString message, setNumber, setName, source, fingerprint, destination;
        QStringList warnings;
        QList<Row> rows;
        int manufacturerId = 0;
        qint64 requiredPieces = 0, sparePieces = 0, totalPieces = 0;
    };
    struct Result {
        bool success = false, replayed = false, storageCreated = false;
        QString message, destination;
        int workspaceId = 0, storageId = 0;
        qint64 totalPieces = 0;
    };
    explicit CatalogSetPartOutService(QSqlDatabase database) : m_database(database) {}
    Plan preview(const Request& request) const;
    Result execute(const Request& request, const QString& fingerprint) const;
    // Called on a worker thread: open/close a dedicated connection on that thread.
    static Plan previewFile(const QString& databasePath, const Request& request);
    static Result executeFile(const QString& databasePath, const Request& request, const QString& fingerprint);
private:
    QSqlDatabase m_database;
};

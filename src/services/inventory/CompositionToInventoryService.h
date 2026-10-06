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
#include <QString>

// All writes belong to the caller's transaction. No Collection/Build lifecycle.
class CompositionToInventoryService
{
public:
    struct Row { int partId = 0; int colorId = 0; int storageId = 0; qint64 quantity = 0; };
    struct Context {
        int workspaceId = 0;
        int manufacturerId = 0;
        QString condition;
        QString ownership = QStringLiteral("Owned");
        QString movementType, referenceType, referenceId, notes;
    };
    struct Result {
        bool success = false;
        QString message;
        qint64 totalPieces = 0;
        QList<int> inventoryIds;
    };
    explicit CompositionToInventoryService(QSqlDatabase database) : m_database(database) {}
    Result validate(const Context& context, const QList<Row>& rows) const;
    Result addInCurrentTransaction(const Context& context, const QList<Row>& rows) const;
private:
    QSqlDatabase m_database;
};

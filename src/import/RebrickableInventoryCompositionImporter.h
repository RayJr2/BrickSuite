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

#include "global/RebrickableImportCancellation.h"

#include <QHash>
#include <QString>

class QSqlDatabase;

class RebrickableInventoryCompositionImporter
{
public:
    struct Result {
        bool success = false;
        qint64 rowsRead = 0;
        qint64 inserted = 0;
        qint64 updated = 0;
        qint64 unchanged = 0;
        qint64 replaced = 0;
        qint64 deactivated = 0;
        qint64 preferredChanged = 0;
        qint64 setInventoryRows = 0;
        qint64 recognizedMinifigInventories = 0;
        qint64 minifigPartRows = 0;
        QString message;
    };

    Result importInventories(const QString& fileName, QSqlDatabase& database,
                             const RebrickableImportCancellation* cancellation = nullptr,
                             const RebrickableRowProgress& progress = {});
    Result importParts(const QString& fileName, QSqlDatabase& database,
                       const RebrickableImportCancellation* cancellation = nullptr,
                       const RebrickableRowProgress& progress = {});
    Result importMinifigs(const QString& fileName, QSqlDatabase& database,
                          const RebrickableImportCancellation* cancellation = nullptr,
                          const RebrickableRowProgress& progress = {});
    Result importSets(const QString& fileName, QSqlDatabase& database,
                      const RebrickableImportCancellation* cancellation = nullptr,
                      const RebrickableRowProgress& progress = {});
    Result selectPreferredRevisions(QSqlDatabase& database) const;

private:
    // True identifies a Set inventory; false identifies a Minifig inventory.
    // This exact provider ownership map lives only for one global import run.
    QHash<QString, bool> m_inventoryOwners;
    QHash<QString, int> m_minifigInventoryOwners;
};

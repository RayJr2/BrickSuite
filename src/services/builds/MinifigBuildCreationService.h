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

#include <QString>
#include <QSqlDatabase>

class MinifigBuildCreationService
{
public:
    struct Result
    {
        bool success = false;
        int buildId = 0;
        int requirementRows = 0;
        int requiredPieces = 0;
        QString message;
    };

    MinifigBuildCreationService();
    explicit MinifigBuildCreationService(const QSqlDatabase& database);
    Result create(int workspaceId, int minifigCatalogId, const QString& buildName) const;
    Result createInCurrentTransaction(int workspaceId, int minifigCatalogId,
                                      const QString& buildName) const;
private:
    QSqlDatabase database() const;
    QString m_connectionName;
};

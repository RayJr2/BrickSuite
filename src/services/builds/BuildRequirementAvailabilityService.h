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

class BuildRequirement;
class QSqlDatabase;
class QThread;

class BuildRequirementAvailabilityService
{
public:
    struct Projection {
        int owned = 0;
        int thisRequirementAllocated = 0;
        int otherAllocated = 0;
        int available = 0;
        int missing = 0;
    };

    BuildRequirementAvailabilityService();
    explicit BuildRequirementAvailabilityService(const QSqlDatabase& database);
    Projection project(int workspaceId, const BuildRequirement& requirement) const;

private:
    QSqlDatabase serviceDatabase() const;
    QString m_connectionName;
    QThread* m_ownerThread = nullptr;
};

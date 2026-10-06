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

#include "RepositoryConnection.h"

#include "../models/BuildRequirement.h"

#include <QList>
#include <optional>

class QSqlQuery;

class BuildRequirementRepository : protected RepositoryConnection
{
public:
    BuildRequirementRepository() = default;
    explicit BuildRequirementRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}

    bool create(BuildRequirement& requirement);
    std::optional<BuildRequirement> getById(int id) const;
    bool tryGetById(int id, std::optional<BuildRequirement>& requirement) const;
    QList<BuildRequirement> getByBuild(int buildId) const;
    bool tryGetByBuild(int buildId, QList<BuildRequirement>& requirements) const;
    bool update(BuildRequirement& requirement);
    bool remove(int requirementId);
    bool removeAllForBuild(int buildId);
    std::optional<BuildRequirement> getByBuildPartColor(int buildId,
                                                        int partId,
                                                        int colorId,
                                                        bool isSpare) const;

private:
    BuildRequirement requirementFromQuery(const QSqlQuery& query) const;
};

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

#include "../models/PartRelationship.h"

#include <QList>
#include <QString>

class PartRelationshipRepository
{
public:
    QList<PartRelationship> getByParentPartId(int parentPartId) const;
    QList<PartRelationship> getByChildPartId(int childPartId) const;
    QList<PartRelationship> getByPartId(int partId) const;
    QList<int> getActiveDecoratedChildPartIdsByParentPartId(
        int parentPartId,
        int limit) const;

    bool upsert(const PartRelationship& relationship) const;

    bool setActive(int relationshipId, bool active) const;
    bool setAllActiveForSource(const QString& source, bool active) const;
};

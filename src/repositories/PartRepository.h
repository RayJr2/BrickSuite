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

#include "../models/Part.h"
#include "../models/PartSearchCriteria.h"
#include "../models/PartSearchResult.h"

#include <QList>
#include <optional>

class QSqlQuery;

class PartRepository
{
public:
    bool create(Part& part);

    QList<Part> getAll() const;

    std::optional<Part> getById(int id) const;

    std::optional<Part> getByPartNumber(const QString& partNumber) const;

    bool update(Part& part);

    QList<PartSearchResult> search(const PartSearchCriteria& criteria) const;

    int count(const PartSearchCriteria& criteria) const;

    QList<Part> searchForInventoryEntry(const QString& searchText, int limit = 20) const;

    // M23.3 Part Reference: retrieve active, non-printed parts from one or
    // more local Part Category IDs. Used only for the curated reference
    // galleries, not as a replacement for Parts Catalog search.
    QList<Part> getReferencePartsByCategoryIds(const QList<int>& categoryIds,
                                               int limit = 160) const;

private:
    Part partFromQuery(const QSqlQuery& query) const;
};

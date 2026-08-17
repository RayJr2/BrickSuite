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

#include "../models/SetCatalogItem.h"
#include "../models/SetCatalogSearchCriteria.h"

#include <QList>
#include <optional>

class QSqlQuery;

class SetCatalogRepository
{
public:
    std::optional<SetCatalogItem> getById(int id) const;

    std::optional<SetCatalogItem> getBySetNumber(const QString& setNumber) const;

    QList<SetCatalogItem> search(const SetCatalogSearchCriteria& criteria) const;

    int count(const SetCatalogSearchCriteria& criteria) const;

    QList<int> getYears() const;

    int count() const;

private:
    SetCatalogItem setFromQuery(const QSqlQuery& query) const;
};

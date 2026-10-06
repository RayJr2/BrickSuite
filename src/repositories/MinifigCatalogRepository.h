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
#include "../models/MinifigCatalogItem.h"
#include "../models/MinifigCatalogSearchCriteria.h"
#include "../models/MinifigCatalogSearchResult.h"
#include "../models/MinifigExternalIdentifier.h"

#include <QList>
#include <optional>

class QSqlQuery;

class MinifigCatalogRepository : protected RepositoryConnection
{
public:
    MinifigCatalogRepository() = default;
    explicit MinifigCatalogRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}
    std::optional<MinifigCatalogItem> getById(int id) const;
    bool tryGetById(int id, std::optional<MinifigCatalogItem>& item) const;
    std::optional<MinifigCatalogItem> getByExternalIdentifier(
        const QString& provider,
        const QString& externalId,
        bool* querySucceeded = nullptr) const;
    QList<MinifigExternalIdentifier> identifiersForMinifig(int minifigCatalogId,
                                                           bool activeOnly = true) const;
    QList<MinifigCatalogSearchResult> search(
        const MinifigCatalogSearchCriteria& criteria) const;
    int count(const MinifigCatalogSearchCriteria& criteria) const;
    int count(bool includeInactive = false) const;

private:
    static MinifigCatalogItem itemFromQuery(const QSqlQuery& query);
};

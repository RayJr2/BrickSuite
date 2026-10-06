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

#include "../models/CollectionItem.h"
#include "../models/CollectionSearchCriteria.h"
#include "../models/CollectionSearchResult.h"

#include <QList>
#include <optional>

class QSqlQuery;

class CollectionRepository : protected RepositoryConnection
{
public:
    CollectionRepository() = default;
    explicit CollectionRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}

    bool create(CollectionItem& item);
    std::optional<CollectionItem> getById(int id) const;
    bool tryGetById(int id, std::optional<CollectionItem>& item) const;
    std::optional<CollectionItem> getBySourceBuild(int buildId) const;
    bool tryGetBySourceBuild(int buildId, std::optional<CollectionItem>& item) const;
    bool hasSourceBuild(int buildId) const;
    bool tryHasSourceBuild(int buildId, bool& found) const;
    QList<CollectionSearchResult> search(const CollectionSearchCriteria& criteria) const;
    std::optional<CollectionSearchResult> displayById(int id) const;
    int count(const CollectionSearchCriteria& criteria) const;
    bool update(CollectionItem& item);
    bool transitionCatalogItemToUnassembled(int itemId, int workspaceId,
        const QDateTime& expectedModifiedUtc);
    bool updateStateForSourceBuild(int buildId, CollectionItemState state);
    bool setActive(int itemId, bool active);

private:
    static CollectionItem itemFromQuery(const QSqlQuery& query);
};

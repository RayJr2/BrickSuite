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

struct SetInventoryRevision
{
    int id = 0;
    QString provider;
    QString externalInventoryId;
    int setCatalogId = 0;
    int version = 0;
    bool active = false;
    bool preferred = false;
};

struct SetInventoryPart
{
    int partId = 0;
    int colorId = 0;
    qint64 quantity = 0;
    bool spare = false;
    QString imageUrl;
};

struct SetInventoryMinifig
{
    int minifigCatalogId = 0;
    qint64 quantity = 0;
};

struct SetInventoryContainedSet
{
    int setCatalogId = 0;
    qint64 quantity = 0;
};

class SetInventoryRevisionRepository
{
public:
    explicit SetInventoryRevisionRepository(
        QSqlDatabase database = QSqlDatabase());
    QList<SetInventoryRevision> revisionsForSet(int setCatalogId,
                                                const QString& provider = {}) const;
    SetInventoryRevision preferredRevisionForSet(int setCatalogId,
                                                  const QString& provider) const;
    QList<SetInventoryPart> partsForRevision(int revisionId,
                                             bool includeSpares = true) const;
    QList<SetInventoryMinifig> minifigsForRevision(int revisionId) const;
    QList<SetInventoryContainedSet> containedSetsForRevision(int revisionId) const;
    QList<int> revisionIdsContainingPart(int partId, int colorId) const;
    QList<int> revisionIdsContainingMinifig(int minifigCatalogId) const;
    QList<int> revisionIdsContainingSet(int setCatalogId) const;

private:
    QSqlDatabase m_database;
};

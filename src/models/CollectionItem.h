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

#include <QDateTime>
#include <QString>

enum class CollectionItemType { Set, Minifig, Moc, Invalid };
enum class CollectionItemState { Assembled, Unassembled, PartiallyAssembled, Sealed, Invalid };
enum class CollectionItemCondition { New, Used, Invalid };
enum class CollectionItemCompleteness { Unknown, Complete, Incomplete, Invalid };

QString collectionItemTypeToString(CollectionItemType type);
CollectionItemType collectionItemTypeFromString(const QString& value);
QString collectionItemStateToString(CollectionItemState state);
CollectionItemState collectionItemStateFromString(const QString& value);
QString collectionItemConditionToString(CollectionItemCondition condition);
CollectionItemCondition collectionItemConditionFromString(const QString& value);
QString collectionItemCompletenessToString(CollectionItemCompleteness completeness);
CollectionItemCompleteness collectionItemCompletenessFromString(const QString& value);

class CollectionItem
{
public:
    int id = 0;
    int workspaceId = 0;
    CollectionItemType type = CollectionItemType::Invalid;
    int setCatalogId = 0;
    int minifigCatalogId = 0;
    CollectionItemState state = CollectionItemState::Invalid;
    CollectionItemCondition condition = CollectionItemCondition::Used;
    CollectionItemCompleteness completeness = CollectionItemCompleteness::Unknown;
    int storageLocationId = 0;
    int sourceBuildId = 0;
    QString nickname;
    QString notes;
    bool allowPartsSource = false;
    bool isActive = true;
    QDateTime createdUtc;
    QDateTime modifiedUtc;
};

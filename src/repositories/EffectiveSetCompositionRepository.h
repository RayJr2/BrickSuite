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

enum class EffectiveSetCompositionSource
{
    None,
    PreferredRebrickableRevision,
    LegacyCatalogFallback
};

struct EffectiveSetCompositionPart
{
    int id = 0;
    int partId = 0;
    int colorId = 0;
    qint64 quantity = 0;
    bool spare = false;
    QString partNumber;
    QString partName;
    QString colorName;
    int rebrickableColorId = 0;
};

struct EffectiveSetComposition
{
    bool success = false;
    EffectiveSetCompositionSource source = EffectiveSetCompositionSource::None;
    int revisionId = 0;
    int revisionVersion = 0;
    QString provider;
    QString message;
    QList<EffectiveSetCompositionPart> parts;
};

class EffectiveSetCompositionRepository
{
public:
    explicit EffectiveSetCompositionRepository(QSqlDatabase database = QSqlDatabase());
    EffectiveSetComposition forSet(int setCatalogId, bool includeSpares = true) const;

private:
    QSqlDatabase m_database;
};

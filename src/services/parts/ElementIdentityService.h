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

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include <optional>

struct ElementIdentityResult
{
    bool applicable = false;
    QStringList elementIds;

    QString label() const;
    QString displayText() const;
};

struct ElementPartColorIdentity
{
    int partId = 0;
    int colorId = 0;
};

class ElementIdentityService
{
public:
    explicit ElementIdentityService(QSqlDatabase database = QSqlDatabase());

    ElementIdentityResult forInventory(int partId, int colorId,
                                       int manufacturerId) const;
    QStringList forPartColor(int partId, int colorId) const;
    std::optional<ElementPartColorIdentity> fromElementId(
        const QString& elementId) const;

private:
    QSqlDatabase m_database;
};

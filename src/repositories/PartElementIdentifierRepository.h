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

struct PartElementIdentifier
{
    int id = 0;
    QString provider;
    QString elementId;
    int partId = 0;
    int colorId = 0;
    QString designId;
    bool active = false;
};

class PartElementIdentifierRepository
{
public:
    explicit PartElementIdentifierRepository(QSqlDatabase database = QSqlDatabase());
    PartElementIdentifier findByElementId(const QString& provider,
                                          const QString& elementId) const;
    QList<PartElementIdentifier> findByPartColor(int partId, int colorId,
                                                 const QString& provider = {}) const;

private:
    QSqlDatabase m_database;
};

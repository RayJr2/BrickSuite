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

#include <QString>

class StorageLocationType
{
public:
    StorageLocationType() = default;

    int id() const;
    void setId(int id);

    QString name() const;
    void setName(const QString& name);

    QString description() const;
    void setDescription(const QString& description);

    bool isSystem() const;
    void setIsSystem(bool isSystem);

    bool isActive() const;
    void setIsActive(bool isActive);

    int sortOrder() const;
    void setSortOrder(int sortOrder);

private:
    int m_id = 0;
    QString m_name;
    QString m_description;
    bool m_isSystem = false;
    bool m_isActive = true;
    int m_sortOrder = 0;
};
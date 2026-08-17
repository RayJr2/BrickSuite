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

class StorageLocation
{
public:
    StorageLocation() = default;

    int id() const;
    void setId(int id);

    int workspaceId() const;
    void setWorkspaceId(int workspaceId);

    int parentLocationId() const;
    void setParentLocationId(int parentLocationId);

    int locationTypeId() const;
    void setLocationTypeId(int locationTypeId);

    QString name() const;
    void setName(const QString& name);

    QString description() const;
    void setDescription(const QString& description);

    int sortOrder() const;
    void setSortOrder(int sortOrder);

    bool isActive() const;
    void setIsActive(bool isActive);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;
    int m_workspaceId = 0;
    int m_parentLocationId = 0;
    int m_locationTypeId = 0;

    QString m_name;
    QString m_description;

    int m_sortOrder = 0;
    bool m_isActive = true;

    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};
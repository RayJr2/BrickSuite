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

class InventoryRecord
{
public:
    InventoryRecord() = default;

    int id() const;
    void setId(int id);

    int workspaceId() const;
    void setWorkspaceId(int workspaceId);

    int partId() const;
    void setPartId(int partId);

    int colorId() const;
    void setColorId(int colorId);

    int storageLocationId() const;
    void setStorageLocationId(int storageLocationId);

    int manufacturerId() const;
    void setManufacturerId(int manufacturerId);

    QString condition() const;
    void setCondition(const QString& condition);

    QString ownershipType() const;
    void setOwnershipType(const QString& ownershipType);

    int quantity() const;
    void setQuantity(int quantity);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;
    int m_workspaceId = 0;
    int m_partId = 0;
    int m_colorId = 0;
    int m_storageLocationId = 0;
    int m_manufacturerId = 0;

    QString m_condition = "Used";
    QString m_ownershipType = "Owned";

    int m_quantity = 0;

    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};

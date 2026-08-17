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

class BuildAllocation
{
public:
    BuildAllocation() = default;

    int id() const;
    void setId(int id);

    int buildId() const;
    void setBuildId(int buildId);

    int inventoryRecordId() const;
    void setInventoryRecordId(int inventoryRecordId);

    int partId() const;
    void setPartId(int partId);

    int colorId() const;
    void setColorId(int colorId);

    int storageLocationId() const;
    void setStorageLocationId(int storageLocationId);

    int quantityAllocated() const;
    void setQuantityAllocated(int quantityAllocated);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;

    int m_buildId = 0;

    int m_inventoryRecordId = 0;

    int m_partId = 0;
    int m_colorId = 0;

    int m_storageLocationId = 0;

    int m_quantityAllocated = 0;

    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};
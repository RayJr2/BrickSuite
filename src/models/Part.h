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

class Part
{
public:
    Part() = default;

    int id() const;
    void setId(int id);

    QString partNumber() const;
    void setPartNumber(const QString& partNumber);

    QString name() const;
    void setName(const QString& name);

    int partCategoryId() const;
    void setPartCategoryId(int partCategoryId);

    QString rebrickablePartId() const;
    void setRebrickablePartId(const QString& rebrickablePartId);

    bool isActive() const;
    void setIsActive(bool isActive);

    QString material() const;
    void setMaterial(const QString& material);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;
    QString m_partNumber;
    QString m_name;
    int m_partCategoryId = 0;
    QString m_rebrickablePartId;
    bool m_isActive = true;
    QString m_material;
    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};
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

class Build
{
public:
    Build() = default;

    int id() const;
    void setId(int id);

    int workspaceId() const;
    void setWorkspaceId(int workspaceId);

    QString buildType() const;
    void setBuildType(const QString& buildType);

    QString name() const;
    void setName(const QString& name);

    QString setNumber() const;
    void setSetNumber(const QString& setNumber);

    int minifigCatalogId() const;
    void setMinifigCatalogId(int minifigCatalogId);

    QString sourceReference() const;
    void setSourceReference(const QString& sourceReference);

    QString inventoryMode() const;
    void setInventoryMode(const QString& inventoryMode);

    int manufacturerId() const;
    void setManufacturerId(int manufacturerId);

    QString status() const;
    void setStatus(const QString& status);

    bool isActive() const;
    void setIsActive(bool active);

    QString notes() const;
    void setNotes(const QString& notes);

    QDateTime createdUtc() const;
    void setCreatedUtc(const QDateTime& createdUtc);

    QDateTime modifiedUtc() const;
    void setModifiedUtc(const QDateTime& modifiedUtc);

private:
    int m_id = 0;
    int m_workspaceId = 0;

    QString m_buildType = "Set";
    QString m_name;
    QString m_setNumber;
    int m_minifigCatalogId = 0;
    QString m_sourceReference;
    QString m_inventoryMode = "Stock";
    int m_manufacturerId = 0;
    QString m_status = "Planned";
    bool m_isActive = true;
    QString m_notes;

    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
};

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

#include "RepositoryConnection.h"
#include "../models/Manufacturer.h"

#include <QList>
#include <QString>
#include <optional>

class QSqlQuery;

enum class ManufacturerIdentityConflict { None, Code, Name, DatabaseError };

struct ManufacturerUsage
{
    bool success = true;
    QString errorMessage;
    int inventoryRecordCount = 0;
    int inventoryPieceQuantity = 0;
    int buildCount = 0;
    int provenanceCount = 0;
    int provenancePieceQuantity = 0;

    bool inUse() const
    {
        return inventoryRecordCount > 0
               || buildCount > 0
               || provenanceCount > 0;
    }
};

class ManufacturerRepository : protected RepositoryConnection
{
public:
    ManufacturerRepository() = default;
    explicit ManufacturerRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}
    QList<Manufacturer> getAll(bool activeOnly = true) const;
    std::optional<Manufacturer> getById(int id) const;
    std::optional<Manufacturer> getByCode(const QString& code) const;
    std::optional<Manufacturer> getByName(const QString& name) const;

    bool codeExists(const QString& code, int excludeManufacturerId = 0) const;
    bool nameExists(const QString& name, int excludeManufacturerId = 0) const;
    ManufacturerIdentityConflict identityConflict(const QString& code,
                                                  const QString& name,
                                                  int excludeManufacturerId,
                                                  QString* errorMessage = nullptr) const;

    ManufacturerUsage usage(int manufacturerId) const;

    bool create(Manufacturer& manufacturer) const;
    bool update(Manufacturer& manufacturer) const;
    bool setActive(int id, bool active) const;

    int legoManufacturerId() const;

private:
    static Manufacturer fromQuery(const QSqlQuery& query);
};

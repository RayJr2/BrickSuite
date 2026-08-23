/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../models/Manufacturer.h"

#include <QList>
#include <QString>
#include <optional>

class QSqlQuery;

struct ManufacturerUsage
{
    int inventoryRecordCount = 0;
    int buildCount = 0;
    int provenanceCount = 0;

    bool inUse() const
    {
        return inventoryRecordCount > 0
               || buildCount > 0
               || provenanceCount > 0;
    }
};

class ManufacturerRepository
{
public:
    QList<Manufacturer> getAll(bool activeOnly = true) const;
    std::optional<Manufacturer> getById(int id) const;
    std::optional<Manufacturer> getByCode(const QString& code) const;
    std::optional<Manufacturer> getByName(const QString& name) const;

    bool codeExists(const QString& code, int excludeManufacturerId = 0) const;
    bool nameExists(const QString& name, int excludeManufacturerId = 0) const;

    ManufacturerUsage usage(int manufacturerId) const;

    bool create(Manufacturer& manufacturer) const;
    bool update(Manufacturer& manufacturer) const;
    bool setActive(int id, bool active) const;

    int legoManufacturerId() const;

private:
    static Manufacturer fromQuery(const QSqlQuery& query);
};

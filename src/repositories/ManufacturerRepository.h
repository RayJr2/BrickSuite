/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../models/Manufacturer.h"

#include <QList>
#include <optional>

class QSqlQuery;

class ManufacturerRepository
{
public:
    QList<Manufacturer> getAll(bool activeOnly = true) const;
    std::optional<Manufacturer> getById(int id) const;
    std::optional<Manufacturer> getByCode(const QString& code) const;

    bool create(Manufacturer& manufacturer) const;
    bool update(Manufacturer& manufacturer) const;
    bool setActive(int id, bool active) const;

    int legoManufacturerId() const;

private:
    static Manufacturer fromQuery(const QSqlQuery& query);
};

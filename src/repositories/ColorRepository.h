#pragma once

#include "../models/Color.h"

#include <QList>
#include <optional>

class QSqlQuery;

class ColorRepository
{
public:
    QList<Color> getAll() const;

    std::optional<Color> getById(int id) const;

    std::optional<Color> getByRebrickableId(int rebrickableId) const;

private:
    Color colorFromQuery(const QSqlQuery& query) const;
};
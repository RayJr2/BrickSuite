#pragma once

#include "../models/Part.h"
#include "../models/PartSearchCriteria.h"
#include "../models/PartSearchResult.h"

#include <QList>
#include <optional>

class QSqlQuery;

class PartRepository
{
public:
    bool create(Part& part);

    QList<Part> getAll() const;

    std::optional<Part> getById(int id) const;

    std::optional<Part> getByPartNumber(const QString& partNumber) const;

    bool update(Part& part);

    QList<PartSearchResult> search(const PartSearchCriteria& criteria) const;

private:
    Part partFromQuery(const QSqlQuery& query) const;
};

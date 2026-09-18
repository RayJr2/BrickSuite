#pragma once

#include "../../models/export/MissingPartsExportTypes.h"

#include <QSqlDatabase>

class PickABrickPartResolutionService
{
public:
    explicit PickABrickPartResolutionService(QSqlDatabase database = QSqlDatabase());

    PickABrickPartResolution resolveExact(const QString& partNumber,
                                          int rebrickableColorId) const;

private:
    QSqlDatabase m_database;
};

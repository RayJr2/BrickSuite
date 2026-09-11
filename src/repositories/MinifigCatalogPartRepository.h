#pragma once

#include "RepositoryConnection.h"
#include "../models/MinifigCatalogPart.h"

#include <QList>
#include <QString>

class MinifigCatalogPartRepository : protected RepositoryConnection
{
public:
    MinifigCatalogPartRepository() = default;
    explicit MinifigCatalogPartRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}
    QList<MinifigCatalogPart> listForMinifig(int minifigCatalogId) const;
    bool replaceForMinifig(int minifigCatalogId,
                           const QList<MinifigCatalogPart>& parts,
                           QString& errorMessage) const;
};

#pragma once

#include "../models/MinifigCatalogPart.h"

#include <QList>
#include <QString>

class MinifigCatalogPartRepository
{
public:
    QList<MinifigCatalogPart> listForMinifig(int minifigCatalogId) const;
    bool replaceForMinifig(int minifigCatalogId,
                           const QList<MinifigCatalogPart>& parts,
                           QString& errorMessage) const;
};

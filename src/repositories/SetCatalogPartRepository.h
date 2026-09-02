#pragma once

#include "../models/SetCatalogPart.h"

#include <QList>
#include <QString>

class SetCatalogPartRepository
{
public:
    struct Counts { int rows = 0; int requiredPieces = 0; int sparePieces = 0; };

    QList<SetCatalogPart> listForSet(int setCatalogId) const;
    Counts countsForSet(int setCatalogId) const;
    bool replaceForSet(int setCatalogId,
                       const QList<SetCatalogPart>& parts,
                       QString& errorMessage) const;
};

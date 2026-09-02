#pragma once

#include <QList>
#include <QString>

class MinifigCompositionReplacementService
{
public:
    struct InputRow
    {
        QString rebrickablePartNumber;
        int rebrickableColorId = 0;
        int quantity = 0;
        bool isSpare = false;
        QString context;
    };

    struct Result
    {
        bool success = false;
        int compositionRows = 0;
        int requiredPieces = 0;
        int sparePieces = 0;
        QString message;
    };

    Result replace(int minifigCatalogId,
                   const QList<InputRow>& rows,
                   const QString& provider,
                   const QString& source) const;
};

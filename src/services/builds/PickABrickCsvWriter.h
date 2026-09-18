#pragma once

#include "../../models/export/MissingPartsExportTypes.h"

#include <QByteArray>
#include <QString>

class PickABrickCsvWriter
{
public:
    struct Result
    {
        bool success = false;
        QByteArray csv;
        QString message;
    };

    static Result generate(const PickABrickExportProjection& projection);
    static Result write(const QString& fileName,
                        const PickABrickExportProjection& projection);
};

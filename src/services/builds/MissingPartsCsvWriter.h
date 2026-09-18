#pragma once

#include "../../models/export/MissingPartsExportTypes.h"

#include <QString>

class MissingPartsCsvWriter
{
public:
    struct Result
    {
        bool success = false;
        QString message;
        QString csv;
    };

    static Result generate(const MissingPartsExportProjection& projection);
    static Result write(const QString& fileName,
                        const MissingPartsExportProjection& projection);

private:
    static QString csvField(const QString& value, bool quoteAlways);
};

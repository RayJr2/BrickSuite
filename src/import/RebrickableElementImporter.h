#pragma once

#include "global/RebrickableImportCancellation.h"

#include <QString>

class QSqlDatabase;

class RebrickableElementImporter
{
public:
    struct Result {
        bool success=false;
        qint64 rowsRead=0,inserted=0,updated=0,unchanged=0,reactivated=0,deactivated=0;
        QString message;
    };
    Result importFile(const QString& fileName,QSqlDatabase& database,
                      const RebrickableImportCancellation* cancellation=nullptr,
                      const RebrickableRowProgress& progress={}) const;
};

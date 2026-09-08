#pragma once

#include <QString>
#include "global/RebrickableImportCancellation.h"

class QSqlDatabase;

class RebrickableThemeCatalogImporter
{
public:
    struct Result {
        bool success = false;
        int rowsRead = 0;
        int inserted = 0;
        int updated = 0;
        int unchanged = 0;
        int reactivated = 0;
        int deactivated = 0;
        QString message;
    };

    Result importFile(const QString& fileName, QSqlDatabase& database,
                      const RebrickableImportCancellation* cancellation = nullptr,
                      bool manageTransaction = true,
                      const QString& source = QStringLiteral("Rebrickable themes.csv"),
                      const RebrickableRowProgress& progress = {});
};

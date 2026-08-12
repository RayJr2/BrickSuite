#pragma once

#include <QString>
#include <QStringList>

class QSqlDatabase;

class RebrickableInventoryImporter
{
public:
    struct ImportOptions
    {
        int workspaceId = 0;
        int storageLocationId = 0;

        QString condition = "Used";
        QString ownershipType = "Owned";
    };

    struct ImportResult
    {
        int rowsProcessed = 0;
        int rowsImported = 0;
        int rowsFailed = 0;

        int totalQuantityImported = 0;
    };

    explicit RebrickableInventoryImporter(QSqlDatabase& database);

    bool importOwnedParts(const QString& filePath,
                          const ImportOptions& options,
                          ImportResult& result);

private:
    QStringList parseCsvLine(const QString& line) const;

    QSqlDatabase& m_database;
};
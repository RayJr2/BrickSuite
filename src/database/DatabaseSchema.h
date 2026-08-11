#pragma once

#include <QSqlDatabase>

class DatabaseSchema
{
public:
    static bool initialize(QSqlDatabase& database);

private:
    static constexpr int CurrentSchemaVersion = 2;

    static bool createSchemaVersionTable(QSqlDatabase& database);
    static bool getSchemaVersion(QSqlDatabase& database, int& version);

    static bool setSchemaVersion(QSqlDatabase& database, int version);

    static bool createVersion1Schema(QSqlDatabase& database);
    static bool migrateVersion1ToVersion2(QSqlDatabase& database);

    static bool createWorkspaceTable(QSqlDatabase& database);

    static bool createColorTable(QSqlDatabase& database);
    static bool createPartCategoryTable(QSqlDatabase& database);

    static bool createStorageLocationTypeTable(QSqlDatabase& database);

    static bool createStorageLocationTable(QSqlDatabase& database);

    static bool seedStorageLocationTypes(QSqlDatabase& database);
};
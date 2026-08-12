#pragma once

#include <QSqlDatabase>

class DatabaseSchema
{
public:
    static bool initialize(QSqlDatabase& database);

private:
    static constexpr int CurrentSchemaVersion = 5;

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

    static bool migrateVersion2ToVersion3(QSqlDatabase& database);

    static bool createPartTable(QSqlDatabase& database);

    static bool createInventoryRecordTable(QSqlDatabase& database);

    static bool createInventoryIndexes(QSqlDatabase& database);

    static bool migrateVersion3ToVersion4(QSqlDatabase& database);

    static bool migrateVersion4ToVersion5(QSqlDatabase& database);

    static bool createInventoryMovementTable(QSqlDatabase& database);

    static bool createInventoryMovementIndexes(QSqlDatabase& database);
};
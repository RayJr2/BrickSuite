#pragma once

#include <QSqlDatabase>

class DatabaseSchema
{
public:
    static constexpr int CurrentSchemaVersion = 9;

    static bool initialize(QSqlDatabase& database);

private:
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

    static bool migrateVersion5ToVersion6(QSqlDatabase& database);

    static bool createBuildTable(QSqlDatabase& database);

    static bool createBuildRequirementTable(QSqlDatabase& database);

    static bool createBuildAllocationTable(QSqlDatabase& database);

    static bool createBuildIndexes(QSqlDatabase& database);

    static bool migrateVersion6ToVersion7(QSqlDatabase& database);

    static bool migrateVersion7ToVersion8(QSqlDatabase& database);

    static bool migrateVersion8ToVersion9(QSqlDatabase& database);

    static bool createSetCatalogTable(QSqlDatabase& database);
};
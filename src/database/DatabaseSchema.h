/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QSqlDatabase>

class DatabaseSchema
{
public:
    static constexpr int CurrentSchemaVersion = 27;

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

    static bool migrateVersion9ToVersion10(QSqlDatabase& database);

    static bool migrateVersion10ToVersion11(QSqlDatabase& database);
    static bool migrateVersion11ToVersion12(QSqlDatabase& database);
    static bool migrateVersion12ToVersion13(QSqlDatabase& database);
    static bool migrateVersion13ToVersion14(QSqlDatabase& database);
    static bool migrateVersion14ToVersion15(QSqlDatabase& database);
    static bool migrateVersion15ToVersion16(QSqlDatabase& database);
    static bool migrateVersion16ToVersion17(QSqlDatabase& database);
    static bool migrateVersion17ToVersion18(QSqlDatabase& database);
    static bool migrateVersion18ToVersion19(QSqlDatabase& database);
    static bool migrateVersion19ToVersion20(QSqlDatabase& database);
    static bool migrateVersion20ToVersion21(QSqlDatabase& database);
    static bool migrateVersion21ToVersion22(QSqlDatabase& database);
    static bool migrateVersion22ToVersion23(QSqlDatabase& database);
    static bool migrateVersion23ToVersion24(QSqlDatabase& database);
    static bool migrateVersion24ToVersion25(QSqlDatabase& database);
    static bool migrateVersion25ToVersion26(QSqlDatabase& database);
    static bool migrateVersion26ToVersion27(QSqlDatabase& database);

    static bool createExternalColorMappingTable(QSqlDatabase& database);
    static bool createExternalPartMappingTable(QSqlDatabase& database);

    static bool createPartRelationshipTable(QSqlDatabase& database);
    static bool createPartAliasTable(QSqlDatabase& database);
    static bool createExternalPartIdentifierTable(QSqlDatabase& database);

    static bool createManufacturerTable(QSqlDatabase& database);
    static bool seedManufacturers(QSqlDatabase& database);

    static bool createSetCatalogTable(QSqlDatabase& database);
    static bool createMinifigCatalogTables(QSqlDatabase& database);
    static bool createThemeCatalogTables(QSqlDatabase& database);
    static bool createMinifigCatalogPartTable(QSqlDatabase& database);
};

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

#include "ReferenceDataSeeder.h"

#include "../import/RebrickableReferenceImporter.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

ReferenceDataSeeder::ReferenceDataSeeder(
    QSqlDatabase& database)
    : m_database(database)
{
}

bool ReferenceDataSeeder::getRecordCount(
    const char* tableName,
    int& count) const
{
    QSqlQuery query(m_database);

    const QString sql =
        QString("SELECT COUNT(*) FROM %1")
            .arg(QString::fromLatin1(tableName));

    if (!query.exec(sql))
    {
        qCritical()
            << "Unable to count records in"
            << tableName
            << ":"
            << query.lastError().text();

        return false;
    }

    if (!query.next())
    {
        qCritical()
            << "Unable to retrieve count for"
            << tableName;

        return false;
    }

    count = query.value(0).toInt();

    return true;
}

bool ReferenceDataSeeder::seedIfRequired()
{
    int colorCount = 0;
    int categoryCount = 0;
    int partCount = 0;

    if (!getRecordCount(
            "color",
            colorCount))
    {
        return false;
    }

    if (!getRecordCount(
            "part_category",
            categoryCount))
    {
        return false;
    }

    if (!getRecordCount("part", partCount)) {
        return false;
    }

    if (colorCount > 0 && categoryCount > 0 && partCount > 0) {
        qInfo() << "BrickSuite reference data already seeded."
                << "Colors:" << colorCount << "Categories:" << categoryCount
                << "Parts:" << partCount;

        return true;
    }

    return seedReferenceData();
}

bool ReferenceDataSeeder::seedReferenceData()
{
    if (!m_database.transaction())
    {
        qCritical()
            << "Unable to begin reference-data seed transaction:"
            << m_database.lastError().text();

        return false;
    }

    RebrickableReferenceImporter importer(
        m_database);

    RebrickableReferenceImporter::ImportResult
        colorResult;

    RebrickableReferenceImporter::ImportResult
        categoryResult;

    RebrickableReferenceImporter::ImportResult partResult;

    if (!importer.importPartCategories(":/rebrickable/part_categories.csv", categoryResult, false)) {
        qCritical() << "Unable to seed Rebrickable part categories.";

        m_database.rollback();
        return false;
    }

    if (!importer.importColors(":/rebrickable/colors.csv", colorResult, false)) {
        qCritical() << "Unable to seed Rebrickable colors.";

        m_database.rollback();
        return false;
    }

    if (!importer.importParts(":/rebrickable/parts.csv", partResult, false)) {
        qCritical() << "Unable to seed Rebrickable parts.";

        m_database.rollback();
        return false;
    }

    if (!m_database.commit())
    {
        qCritical()
            << "Unable to commit BrickSuite reference-data seed:"
            << m_database.lastError().text();

        m_database.rollback();
        return false;
    }

    qInfo() << "BrickSuite reference data seeded."
            << "Colors:" << colorResult.recordsImported
            << "Categories:" << categoryResult.recordsImported
            << "Parts:" << partResult.recordsImported;

    return true;
}
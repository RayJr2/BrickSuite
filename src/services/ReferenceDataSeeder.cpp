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

    if (colorCount > 0 &&
        categoryCount > 0)
    {
        qInfo()
            << "BrickSuite reference data already seeded."
            << "Colors:"
            << colorCount
            << "Categories:"
            << categoryCount;

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

    if (!importer.importColors(
            ":/rebrickable/colors.csv",
            colorResult,
            false))
    {
        qCritical()
            << "Unable to seed Rebrickable colors.";

        m_database.rollback();
        return false;
    }

    if (!importer.importPartCategories(
            ":/rebrickable/part_categories.csv",
            categoryResult,
            false))
    {
        qCritical()
            << "Unable to seed Rebrickable part categories.";

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

    qInfo()
        << "BrickSuite reference data seeded."
        << "Colors:"
        << colorResult.recordsImported
        << "Categories:"
        << categoryResult.recordsImported;

    return true;
}
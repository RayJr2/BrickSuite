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

#include "RebrickablePartCatalogImporter.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

namespace {

QStringList parseCsvLine(const QString& line, bool& ok)
{
    QStringList fields;

    QString field;

    bool inQuotes = false;

    ok = true;

    for (int index = 0; index < line.size(); ++index) {
        const QChar ch = line.at(index);

        if (ch == '"') {
            if (inQuotes && index + 1 < line.size() && line.at(index + 1) == '"') {
                field += '"';

                ++index;
            } else {
                inQuotes = !inQuotes;
            }

            continue;
        }

        if (ch == ',' && !inQuotes) {
            fields.append(field);

            field.clear();

            continue;
        }

        field += ch;
    }

    if (inQuotes) {
        ok = false;

        return fields;
    }

    fields.append(field);

    return fields;
}

QString signature(const QString& name, int categoryId, const QString& material)
{
    return QString("%1|%2|%3").arg(name).arg(categoryId).arg(material);
}

} // namespace

RebrickablePartCatalogImporter::Result RebrickablePartCatalogImporter::importFile(
    const QString& fileName)
{
    Result result;

    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.message = "Unable to open parts.csv.";
        qCritical() << "Parts Catalog import failed to open file:"
                    << fileName << file.errorString();

        return result;
    }

    QTextStream stream(&file);

    if (stream.atEnd()) {
        result.message = "parts.csv is empty.";
        qWarning() << "Parts Catalog import rejected: file is empty:" << fileName;

        return result;
    }

    QString headerLine = stream.readLine();

    if (!headerLine.isEmpty() && headerLine.front() == QChar(0xFEFF)) {
        headerLine.remove(0, 1);
    }

    bool headerOk = false;

    const QStringList headers = parseCsvLine(headerLine, headerOk);

    const int partNumberIndex = headers.indexOf("part_num");

    const int nameIndex = headers.indexOf("name");

    const int categoryIndex = headers.indexOf("part_cat_id");

    const int materialIndex = headers.indexOf("part_material");

    if (!headerOk || partNumberIndex < 0 || nameIndex < 0 || categoryIndex < 0
        || materialIndex < 0) {
        result.message = "The selected file is not a supported "
                         "Rebrickable parts.csv file.";
        qWarning() << "Parts Catalog import rejected: unsupported CSV header:"
                   << fileName;

        return result;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    //
    // Rebrickable category ID
    //       ->
    // BrickSuite part_category.id
    //
    QHash<int, int> categoryMap;

    {
        QSqlQuery query(database);

        if (!query.exec(R"(
            SELECT
                id,
                rebrickable_id
            FROM part_category
            WHERE rebrickable_id IS NOT NULL
        )")) {
            result.message = QString("Unable to load Part Category "
                                     "mappings: %1")
                                 .arg(query.lastError().text());

            return result;
        }

        while (query.next()) {
            const int brickSuiteId = query.value("id").toInt();

            const int rebrickableId = query.value("rebrickable_id").toInt();

            categoryMap.insert(rebrickableId, brickSuiteId);
        }
    }

    if (categoryMap.isEmpty()) {
        result.message = "No Rebrickable Part Category mappings "
                         "are available.";

        return result;
    }

    //
    // Load current Parts Catalog so we can distinguish
    // New / Updated / Unchanged without one SELECT
    // for each of 60,000+ rows.
    //
    QHash<QString, QString> existingSignatures;

    {
        QSqlQuery query(database);

        if (!query.exec(R"(
            SELECT
                part_number,
                name,
                part_category_id,
                material
            FROM part
        )")) {
            result.message = QString("Unable to read the existing "
                                     "Parts Catalog: %1")
                                 .arg(query.lastError().text());

            return result;
        }

        while (query.next()) {
            existingSignatures.insert(query.value("part_number").toString(),
                                      signature(query.value("name").toString(),
                                                query.value("part_category_id").toInt(),
                                                query.value("material").toString()));
        }
    }

    if (!database.transaction()) {
        result.message = "Unable to begin Parts Catalog "
                         "import transaction.";

        return result;
    }

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QSqlQuery insertQuery(database);

    if (!insertQuery.prepare(R"(
        INSERT INTO part
        (
            part_number,
            name,
            part_category_id,
            rebrickable_part_id,
            material,
            is_active,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :part_number,
            :name,
            :part_category_id,
            :rebrickable_part_id,
            :material,
            1,
            :created_utc,
            :modified_utc
        )
    )")) {
        database.rollback();

        result.message = QString("Unable to prepare Parts insert: %1")
                             .arg(insertQuery.lastError().text());

        return result;
    }

    QSqlQuery updateQuery(database);

    if (!updateQuery.prepare(R"(
        UPDATE part
        SET
            name = :name,
            part_category_id = :part_category_id,
            rebrickable_part_id = :rebrickable_part_id,
            material = :material,
            is_active = 1,
            modified_utc = :modified_utc
        WHERE part_number = :part_number
    )")) {
        database.rollback();

        result.message = QString("Unable to prepare Parts update: %1")
                             .arg(updateQuery.lastError().text());

        return result;
    }

    while (!stream.atEnd()) {
        const QString line = stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        ++result.rowsRead;

        bool rowOk = false;

        const QStringList fields = parseCsvLine(line, rowOk);

        if (!rowOk || fields.size() != headers.size()) {
            ++result.skipped;

            continue;
        }

        const QString partNumber = fields.at(partNumberIndex).trimmed();

        const QString name = fields.at(nameIndex).trimmed();

        const QString material = fields.at(materialIndex).trimmed();

        bool categoryOk = false;

        const int rebrickableCategoryId = fields.at(categoryIndex).trimmed().toInt(&categoryOk);

        if (partNumber.isEmpty() || name.isEmpty() || !categoryOk) {
            ++result.skipped;

            continue;
        }

        const auto categoryIterator = categoryMap.constFind(rebrickableCategoryId);

        if (categoryIterator == categoryMap.constEnd()) {
            ++result.skipped;

            continue;
        }

        const int brickSuiteCategoryId = categoryIterator.value();

        const QString newSignature = signature(name, brickSuiteCategoryId, material);

        if (!existingSignatures.contains(partNumber)) {
            insertQuery.bindValue(":part_number", partNumber);

            insertQuery.bindValue(":name", name);

            insertQuery.bindValue(":part_category_id", brickSuiteCategoryId);

            //
            // Rebrickable part_num is also the
            // provider identifier used in the
            // existing BrickSuite database.
            //
            insertQuery.bindValue(":rebrickable_part_id", partNumber);

            insertQuery.bindValue(":material", material);

            insertQuery.bindValue(":created_utc", now);

            insertQuery.bindValue(":modified_utc", now);

            if (!insertQuery.exec()) {
                database.rollback();

                result.message = QString("Unable to insert Part %1: %2")
                                     .arg(partNumber, insertQuery.lastError().text());

                return result;
            }

            existingSignatures.insert(partNumber, newSignature);

            ++result.inserted;

            continue;
        }

        if (existingSignatures.value(partNumber) == newSignature) {
            ++result.unchanged;

            continue;
        }

        updateQuery.bindValue(":name", name);

        updateQuery.bindValue(":part_category_id", brickSuiteCategoryId);

        updateQuery.bindValue(":rebrickable_part_id", partNumber);

        updateQuery.bindValue(":material", material);

        updateQuery.bindValue(":modified_utc", now);

        updateQuery.bindValue(":part_number", partNumber);

        if (!updateQuery.exec()) {
            database.rollback();

            result.message = QString("Unable to update Part %1: %2")
                                 .arg(partNumber, updateQuery.lastError().text());

            return result;
        }

        existingSignatures.insert(partNumber, newSignature);

        ++result.updated;
    }

    file.close();

    if (!database.commit()) {
        database.rollback();

        result.message = QString("Unable to commit Parts Catalog import: %1")
                             .arg(database.lastError().text());

        return result;
    }

    result.success = true;

    result.message = "Parts Catalog import completed successfully.";

    qInfo() << "Parts Catalog import completed."
            << "File:" << fileName
            << "RowsRead:" << result.rowsRead
            << "Inserted:" << result.inserted
            << "Updated:" << result.updated
            << "Unchanged:" << result.unchanged
            << "Skipped:" << result.skipped;

    return result;
}
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

#include "RebrickableReferenceImporter.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

RebrickableReferenceImporter::RebrickableReferenceImporter(
    QSqlDatabase& database)
    : m_database(database)
{
}

QStringList RebrickableReferenceImporter::parseCsvLine(
    const QString& line) const
{
    QStringList fields;
    QString currentField;

    bool insideQuotes = false;

    for (qsizetype i = 0; i < line.length(); ++i)
    {
        const QChar character = line.at(i);

        if (character == '"')
        {
            // Two consecutive quotation marks inside a quoted
            // value represent one literal quotation mark.
            if (insideQuotes &&
                i + 1 < line.length() &&
                line.at(i + 1) == '"')
            {
                currentField.append('"');
                ++i;
            }
            else
            {
                insideQuotes = !insideQuotes;
            }
        }
        else if (character == ',' && !insideQuotes)
        {
            fields.append(currentField);
            currentField.clear();
        }
        else
        {
            currentField.append(character);
        }
    }

    fields.append(currentField);

    return fields;
}

bool RebrickableReferenceImporter::validateHeader(
    const QStringList& actualHeader,
    const QStringList& requiredColumns,
    const QString& fileName) const
{
    for (const QString& requiredColumn : requiredColumns)
    {
        if (!actualHeader.contains(
                requiredColumn,
                Qt::CaseInsensitive))
        {
            qCritical()
                << "Rebrickable import:"
                << fileName
                << "is missing required column:"
                << requiredColumn;

            return false;
        }
    }

    return true;
}

bool RebrickableReferenceImporter::importColors(const QString& filePath,
                                                ImportResult& result,
                                                bool manageTransaction)
{
    result = {};

    QFile file(filePath);

    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
    {
        qCritical()
            << "Unable to open Rebrickable colors file:"
            << filePath
            << file.errorString();

        return false;
    }

    QTextStream stream(&file);

    if (stream.atEnd())
    {
        qCritical()
            << "Rebrickable colors file is empty:"
            << filePath;

        return false;
    }

    const QStringList header =
        parseCsvLine(stream.readLine());

    const QStringList requiredColumns =
    {
        "id",
        "name",
        "rgb",
        "is_trans"
    };

    if (!validateHeader(
            header,
            requiredColumns,
            QFileInfo(filePath).fileName()))
    {
        return false;
    }

    const int idIndex =
        header.indexOf("id");

    const int nameIndex =
        header.indexOf("name");

    const int rgbIndex =
        header.indexOf("rgb");

    const int transparentIndex =
        header.indexOf("is_trans");

    if (manageTransaction) {
        if (!m_database.transaction()) {
            qCritical() << "Unable to begin color import transaction:"
                        << m_database.lastError().text();

            return false;
        }
    }

    QSqlQuery query(m_database);

    if (!query.prepare(R"(
        INSERT INTO color
        (
            name,
            rgb,
            is_transparent,
            rebrickable_id,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :name,
            :rgb,
            :is_transparent,
            :rebrickable_id,
            :created_utc,
            :modified_utc
        )

        ON CONFLICT(rebrickable_id)
        DO UPDATE SET
            name = excluded.name,
            rgb = excluded.rgb,
            is_transparent = excluded.is_transparent,
            modified_utc = excluded.modified_utc
    )"))
    {
        qCritical()
            << "Unable to prepare color import:"
            << query.lastError().text();

        if (manageTransaction)
            m_database.rollback();
        return false;
    }

    while (!stream.atEnd())
    {
        const QString line =
            stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        ++result.recordsProcessed;

        const QStringList fields =
            parseCsvLine(line);

        if (fields.size() != header.size())
        {
            ++result.recordsFailed;

            qWarning()
                << "Invalid Rebrickable color row:"
                << result.recordsProcessed;

            continue;
        }

        bool idValid = false;

        const int rebrickableId =
            fields.at(idIndex).toInt(&idValid);

        if (!idValid)
        {
            ++result.recordsFailed;

            qWarning()
                << "Invalid Rebrickable color ID:"
                << fields.at(idIndex);

            continue;
        }

        const QString name =
            fields.at(nameIndex).trimmed();

        const QString rgb =
            fields.at(rgbIndex).trimmed();

        const QString transparentValue =
            fields.at(transparentIndex)
                .trimmed()
                .toLower();

        const bool isTransparent =
            transparentValue == "true" ||
            transparentValue == "1";

        if (name.isEmpty())
        {
            ++result.recordsFailed;
            continue;
        }

        const QString now =
            QDateTime::currentDateTimeUtc()
                .toString(Qt::ISODateWithMs);

        query.bindValue(
            ":name",
            name);

        query.bindValue(
            ":rgb",
            rgb);

        query.bindValue(
            ":is_transparent",
            isTransparent ? 1 : 0);

        query.bindValue(
            ":rebrickable_id",
            rebrickableId);

        query.bindValue(
            ":created_utc",
            now);

        query.bindValue(
            ":modified_utc",
            now);

        if (!query.exec())
        {
            ++result.recordsFailed;

            qWarning()
                << "Unable to import Rebrickable color:"
                << rebrickableId
                << name
                << query.lastError().text();

            continue;
        }

        ++result.recordsImported;
    }

    if (result.recordsFailed > 0)
    {
        qCritical()
            << "Rebrickable color import encountered"
            << result.recordsFailed
            << "failed records.";

        if (manageTransaction)
            m_database.rollback();

        return false;
    }

    if (manageTransaction) {
        if (!m_database.commit()) {
            qCritical() << "Unable to commit Rebrickable color import:"
                        << m_database.lastError().text();

            m_database.rollback();
            return false;
        }
    }

    qInfo() << "Rebrickable colors imported:" << result.recordsImported;

    return true;
}

bool RebrickableReferenceImporter::importPartCategories(const QString& filePath,
                                                        ImportResult& result,
                                                        bool manageTransaction)
{
    result = {};

    QFile file(filePath);

    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
    {
        qCritical()
            << "Unable to open Rebrickable part categories file:"
            << filePath
            << file.errorString();

        return false;
    }

    QTextStream stream(&file);

    if (stream.atEnd())
    {
        qCritical()
            << "Rebrickable part categories file is empty:"
            << filePath;

        return false;
    }

    const QStringList header =
        parseCsvLine(stream.readLine());

    const QStringList requiredColumns =
    {
        "id",
        "name"
    };

    if (!validateHeader(
            header,
            requiredColumns,
            QFileInfo(filePath).fileName()))
    {
        return false;
    }

    const int idIndex =
        header.indexOf("id");

    const int nameIndex =
        header.indexOf("name");

    if (manageTransaction) {
        if (!m_database.transaction()) {
            qCritical() << "Unable to begin color import transaction:"
                        << m_database.lastError().text();

            return false;
        }
    }

    QSqlQuery query(m_database);

    if (!query.prepare(R"(
        INSERT INTO part_category
        (
            name,
            rebrickable_id,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :name,
            :rebrickable_id,
            :created_utc,
            :modified_utc
        )

        ON CONFLICT(rebrickable_id)
        DO UPDATE SET
            name = excluded.name,
            modified_utc = excluded.modified_utc
    )"))
    {
        qCritical()
            << "Unable to prepare category import:"
            << query.lastError().text();

        if (manageTransaction)
            m_database.rollback();
        return false;
    }

    while (!stream.atEnd())
    {
        const QString line =
            stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        ++result.recordsProcessed;

        const QStringList fields =
            parseCsvLine(line);

        if (fields.size() != header.size())
        {
            ++result.recordsFailed;

            qWarning()
                << "Invalid Rebrickable category row:"
                << result.recordsProcessed;

            continue;
        }

        bool idValid = false;

        const int rebrickableId =
            fields.at(idIndex).toInt(&idValid);

        if (!idValid)
        {
            ++result.recordsFailed;
            continue;
        }

        const QString name =
            fields.at(nameIndex).trimmed();

        if (name.isEmpty())
        {
            ++result.recordsFailed;
            continue;
        }

        const QString now =
            QDateTime::currentDateTimeUtc()
                .toString(Qt::ISODateWithMs);

        query.bindValue(
            ":name",
            name);

        query.bindValue(
            ":rebrickable_id",
            rebrickableId);

        query.bindValue(
            ":created_utc",
            now);

        query.bindValue(
            ":modified_utc",
            now);

        if (!query.exec())
        {
            ++result.recordsFailed;

            qWarning()
                << "Unable to import Rebrickable category:"
                << rebrickableId
                << name
                << query.lastError().text();

            continue;
        }

        ++result.recordsImported;
    }

    if (result.recordsFailed > 0)
    {
        qCritical()
            << "Rebrickable category import encountered"
            << result.recordsFailed
            << "failed records.";

        if (manageTransaction)
            m_database.rollback();

        return false;
    }

    if (manageTransaction) {
        if (!m_database.commit()) {
            qCritical() << "Unable to commit Rebrickable color import:"
                        << m_database.lastError().text();

            m_database.rollback();
            return false;
        }
    }

    qInfo()
        << "Rebrickable part categories imported:"
        << result.recordsImported;

    return true;
}

bool RebrickableReferenceImporter::importParts(const QString& filePath,
                                               ImportResult& result,
                                               bool manageTransaction)
{
    result = {};

    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Unable to open Rebrickable parts file:" << filePath << file.errorString();

        return false;
    }

    QTextStream stream(&file);

    if (stream.atEnd()) {
        qCritical() << "Rebrickable parts file is empty:" << filePath;

        return false;
    }

    const QStringList header = parseCsvLine(stream.readLine());

    const QStringList requiredColumns = {"part_num", "name", "part_cat_id", "part_material"};

    if (!validateHeader(header, requiredColumns, QFileInfo(filePath).fileName())) {
        return false;
    }

    const int partNumberIndex = header.indexOf("part_num");

    const int nameIndex = header.indexOf("name");

    const int categoryIndex = header.indexOf("part_cat_id");

    const int materialIndex = header.indexOf("part_material");

    // Build:
    //
    // Rebrickable category ID
    //          ->
    // BrickSuite part_category.id
    //
    QHash<int, int> categoryMap;

    {
        QSqlQuery categoryQuery(m_database);

        if (!categoryQuery.exec(R"(
            SELECT
                id,
                rebrickable_id
            FROM part_category
            WHERE rebrickable_id IS NOT NULL
        )")) {
            qCritical() << "Unable to load part-category mapping:"
                        << categoryQuery.lastError().text();

            return false;
        }

        while (categoryQuery.next()) {
            const int brickSuiteId = categoryQuery.value("id").toInt();

            const int rebrickableId = categoryQuery.value("rebrickable_id").toInt();

            categoryMap.insert(rebrickableId, brickSuiteId);
        }
    }

    if (categoryMap.isEmpty()) {
        qCritical() << "Cannot import Rebrickable parts: "
                       "no part-category mappings are available.";

        return false;
    }

    if (manageTransaction) {
        if (!m_database.transaction()) {
            qCritical() << "Unable to begin parts import transaction:"
                        << m_database.lastError().text();

            return false;
        }
    }

    QSqlQuery query(m_database);

    if (!query.prepare(R"(
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

        ON CONFLICT(rebrickable_part_id)
        DO UPDATE SET
            part_number = excluded.part_number,
            name = excluded.name,
            part_category_id = excluded.part_category_id,
            material = excluded.material,
            is_active = 1,
            modified_utc = excluded.modified_utc
    )")) {
        qCritical() << "Unable to prepare Rebrickable parts import:" << query.lastError().text();

        if (manageTransaction)
            m_database.rollback();

        return false;
    }

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    while (!stream.atEnd()) {
        const QString line = stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        ++result.recordsProcessed;

        const QStringList fields = parseCsvLine(line);

        if (fields.size() != header.size()) {
            ++result.recordsFailed;

            qWarning() << "Invalid Rebrickable part row:" << result.recordsProcessed;

            continue;
        }

        const QString partNumber = fields.at(partNumberIndex).trimmed();

        const QString name = fields.at(nameIndex).trimmed();

        const QString material = fields.at(materialIndex).trimmed();

        bool categoryValid = false;

        const int rebrickableCategoryId = fields.at(categoryIndex).toInt(&categoryValid);

        if (partNumber.isEmpty() || name.isEmpty() || !categoryValid) {
            ++result.recordsFailed;

            qWarning() << "Invalid Rebrickable part:" << partNumber;

            continue;
        }

        const auto categoryIterator = categoryMap.constFind(rebrickableCategoryId);

        if (categoryIterator == categoryMap.constEnd()) {
            ++result.recordsFailed;

            qWarning() << "Unknown Rebrickable category" << rebrickableCategoryId << "for part"
                       << partNumber;

            continue;
        }

        const int brickSuiteCategoryId = categoryIterator.value();

        query.bindValue(":part_number", partNumber);

        query.bindValue(":name", name);

        query.bindValue(":part_category_id", brickSuiteCategoryId);

        // Rebrickable's part_num is the provider's
        // part identifier.
        query.bindValue(":rebrickable_part_id", partNumber);

        query.bindValue(":material", material);

        query.bindValue(":created_utc", now);

        query.bindValue(":modified_utc", now);

        if (!query.exec()) {
            ++result.recordsFailed;

            qWarning() << "Unable to import Rebrickable part:" << partNumber
                       << query.lastError().text();

            continue;
        }

        ++result.recordsImported;
    }

    if (result.recordsFailed > 0) {
        qCritical() << "Rebrickable parts import encountered" << result.recordsFailed
                    << "failed records out of" << result.recordsProcessed;

        if (manageTransaction)
            m_database.rollback();

        return false;
    }

    if (manageTransaction) {
        if (!m_database.commit()) {
            qCritical() << "Unable to commit Rebrickable parts import:"
                        << m_database.lastError().text();

            m_database.rollback();
            return false;
        }
    }

    qInfo() << "Rebrickable parts imported:" << result.recordsImported;

    return true;
}
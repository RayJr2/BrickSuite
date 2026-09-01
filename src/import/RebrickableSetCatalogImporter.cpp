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

#include "RebrickableSetCatalogImporter.h"

#include "RebrickableCsvInputResolver.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QTemporaryDir>

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

QString signature(const QString& name, int year, int themeId, int numParts, const QString& imageUrl)
{
    return QString("%1|%2|%3|%4|%5").arg(name).arg(year).arg(themeId).arg(numParts).arg(imageUrl);
}
} // namespace

RebrickableSetCatalogImporter::Result RebrickableSetCatalogImporter::importFile(
    const QString& fileName)
{
    Result result;

    QTemporaryDir temporaryDirectory;
    QString csvFileName;

    if (!RebrickableCsvInputResolver::resolve(fileName,
                                              "sets.csv",
                                              temporaryDirectory,
                                              csvFileName,
                                              result.message)) {
        qWarning() << "Set Catalog import rejected input:"
                   << fileName << result.message;
        return result;
    }

    QFile file(csvFileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.message = "Unable to open sets.csv.";
        qCritical() << "Set Catalog import failed to open file:"
                    << fileName << file.errorString();

        return result;
    }

    QTextStream stream(&file);

    if (stream.atEnd()) {
        result.message = "sets.csv is empty.";
        qWarning() << "Set Catalog import rejected: file is empty:" << fileName;

        return result;
    }

    QString header = stream.readLine();

    if (!header.isEmpty() && header.front() == QChar(0xFEFF)) {
        header.remove(0, 1);
    }

    bool headerOk = false;

    const QStringList headers = parseCsvLine(header, headerOk);

    const QStringList expectedHeaders
        = {"set_num", "name", "year", "theme_id", "num_parts", "img_url"};

    if (!headerOk || headers != expectedHeaders) {
        result.message = "The selected file is not a supported "
                         "Rebrickable sets.csv file.";
        qWarning() << "Set Catalog import rejected: unsupported CSV header:"
                   << fileName;

        return result;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    //
    // Load the current catalog into memory so we can
    // classify New / Updated / Unchanged efficiently.
    //
    QHash<QString, QString> existingSignatures;

    {
        QSqlQuery query(database);

        if (!query.exec(R"(
            SELECT
                set_number,
                name,
                year,
                theme_id,
                num_parts,
                image_url
            FROM set_catalog
        )")) {
            result.message = QString("Unable to read the existing "
                                     "Set Catalog: %1")
                                 .arg(query.lastError().text());

            return result;
        }

        while (query.next()) {
            existingSignatures.insert(query.value(0).toString(),
                                      signature(query.value(1).toString(),
                                                query.value(2).toInt(),
                                                query.value(3).toInt(),
                                                query.value(4).toInt(),
                                                query.value(5).toString()));
        }
    }

    if (!database.transaction()) {
        result.message = "Unable to begin Set Catalog import "
                         "transaction.";

        return result;
    }

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QSqlQuery insertQuery(database);

    insertQuery.prepare(R"(
        INSERT INTO set_catalog
        (
            set_number,
            name,
            year,
            theme_id,
            num_parts,
            image_url,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :set_number,
            :name,
            :year,
            :theme_id,
            :num_parts,
            :image_url,
            :created_utc,
            :modified_utc
        )
    )");

    QSqlQuery updateQuery(database);

    updateQuery.prepare(R"(
        UPDATE set_catalog
        SET
            name = :name,
            year = :year,
            theme_id = :theme_id,
            num_parts = :num_parts,
            image_url = :image_url,
            modified_utc = :modified_utc
        WHERE set_number = :set_number
    )");

    while (!stream.atEnd()) {
        const QString line = stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        ++result.rowsRead;

        bool rowOk = false;

        const QStringList fields = parseCsvLine(line, rowOk);

        if (!rowOk || fields.size() != 6) {
            ++result.skipped;
            continue;
        }

        const QString setNumber = fields.at(0).trimmed();

        const QString name = fields.at(1).trimmed();

        bool yearOk = false;
        bool themeOk = false;
        bool partsOk = false;

        const int year = fields.at(2).trimmed().toInt(&yearOk);

        const int themeId = fields.at(3).trimmed().toInt(&themeOk);

        const int numParts = fields.at(4).trimmed().toInt(&partsOk);

        const QString imageUrl = fields.at(5).trimmed();

        if (setNumber.isEmpty() || name.isEmpty() || !yearOk || !themeOk || !partsOk || year < 0
            || themeId < 0 || numParts < 0) {
            ++result.skipped;
            continue;
        }

        const QString newSignature = signature(name, year, themeId, numParts, imageUrl);

        if (!existingSignatures.contains(setNumber)) {
            insertQuery.bindValue(":set_number", setNumber);

            insertQuery.bindValue(":name", name);

            insertQuery.bindValue(":year", year);

            insertQuery.bindValue(":theme_id", themeId);

            insertQuery.bindValue(":num_parts", numParts);

            insertQuery.bindValue(":image_url", imageUrl);

            insertQuery.bindValue(":created_utc", now);

            insertQuery.bindValue(":modified_utc", now);

            if (!insertQuery.exec()) {
                database.rollback();

                result.message = QString("Unable to insert Set %1: %2")
                                     .arg(setNumber, insertQuery.lastError().text());

                return result;
            }

            existingSignatures.insert(setNumber, newSignature);

            ++result.inserted;

            continue;
        }

        if (existingSignatures.value(setNumber) == newSignature) {
            ++result.unchanged;
            continue;
        }

        updateQuery.bindValue(":name", name);

        updateQuery.bindValue(":year", year);

        updateQuery.bindValue(":theme_id", themeId);

        updateQuery.bindValue(":num_parts", numParts);

        updateQuery.bindValue(":image_url", imageUrl);

        updateQuery.bindValue(":modified_utc", now);

        updateQuery.bindValue(":set_number", setNumber);

        if (!updateQuery.exec()) {
            database.rollback();

            result.message = QString("Unable to update Set %1: %2")
                                 .arg(setNumber, updateQuery.lastError().text());

            return result;
        }

        existingSignatures.insert(setNumber, newSignature);

        ++result.updated;
    }

    file.close();

    if (!database.commit()) {
        database.rollback();

        result.message = "Unable to commit Set Catalog import.";

        return result;
    }

    result.success = true;

    result.message = "Set Catalog import completed successfully.";

    qInfo() << "Set Catalog import completed."
            << "File:" << fileName
            << "RowsRead:" << result.rowsRead
            << "Inserted:" << result.inserted
            << "Updated:" << result.updated
            << "Unchanged:" << result.unchanged
            << "Skipped:" << result.skipped;

    return result;
}

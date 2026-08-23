/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "RebrickablePartRelationshipImporter.h"

#include "../database/DatabaseManager.h"
#include "../models/PartRelationship.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

namespace
{
constexpr auto RebrickableSource = "Rebrickable";

QStringList parseCsvLine(const QString& line, bool& ok)
{
    QStringList fields;
    QString field;
    bool inQuotes = false;

    ok = true;

    for (int index = 0; index < line.size(); ++index) {
        const QChar ch = line.at(index);

        if (ch == '"') {
            if (inQuotes
                && index + 1 < line.size()
                && line.at(index + 1) == '"') {
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

PartRelationshipType relationshipTypeForCode(const QString& code)
{
    const QString normalized = code.trimmed().toUpper();

    if (normalized == QStringLiteral("A"))
        return PartRelationshipType::Alternate;
    if (normalized == QStringLiteral("M"))
        return PartRelationshipType::Mold;
    if (normalized == QStringLiteral("P"))
        return PartRelationshipType::Print;
    if (normalized == QStringLiteral("T"))
        return PartRelationshipType::Pattern;
    if (normalized == QStringLiteral("B"))
        return PartRelationshipType::Subpart;
    if (normalized == QStringLiteral("R"))
        return PartRelationshipType::Related;

    return PartRelationshipType::Unknown;
}

QString relationshipKey(int parentPartId,
                        int childPartId,
                        const QString& sourceRelationshipType)
{
    return QStringLiteral("%1|%2|%3")
        .arg(parentPartId)
        .arg(childPartId)
        .arg(sourceRelationshipType.trimmed().toUpper());
}

struct ExistingRelationship
{
    int id = 0;
    QString normalizedType;
    bool active = false;
};
}

RebrickablePartRelationshipImporter::Result
RebrickablePartRelationshipImporter::importFile(
    const QString& fileName)
{
    Result result;

    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.message =
            QStringLiteral("Unable to open part_relationships.csv.");

        qCritical() << "Part Relationship import failed to open file:"
                    << fileName
                    << file.errorString();

        return result;
    }

    QTextStream stream(&file);

    if (stream.atEnd()) {
        result.message =
            QStringLiteral("part_relationships.csv is empty.");
        return result;
    }

    QString headerLine = stream.readLine();

    if (!headerLine.isEmpty()
        && headerLine.front() == QChar(0xFEFF)) {
        headerLine.remove(0, 1);
    }

    bool headerOk = false;
    const QStringList headers =
        parseCsvLine(headerLine, headerOk);

    const int relationTypeIndex =
        headers.indexOf(QStringLiteral("rel_type"));
    const int childPartIndex =
        headers.indexOf(QStringLiteral("child_part_num"));
    const int parentPartIndex =
        headers.indexOf(QStringLiteral("parent_part_num"));

    if (!headerOk
        || relationTypeIndex < 0
        || childPartIndex < 0
        || parentPartIndex < 0) {
        result.message =
            QStringLiteral("The selected file is not a supported "
                           "Rebrickable part_relationships.csv file.");

        qWarning() << "Part Relationship import rejected: unsupported header:"
                   << fileName;

        return result;
    }

    QSqlDatabase database =
        DatabaseManager::instance().database();

    //
    // Build one in-memory Part Number -> BrickSuite part.id map.
    // This avoids tens of thousands of SELECTs during the import.
    //
    QHash<QString, int> partIdByNumber;

    {
        QSqlQuery query(database);

        if (!query.exec(R"(
            SELECT
                id,
                part_number
            FROM part
        )")) {
            result.message =
                QStringLiteral("Unable to load BrickSuite Parts Catalog: %1")
                    .arg(query.lastError().text());

            return result;
        }

        while (query.next()) {
            partIdByNumber.insert(
                query.value(QStringLiteral("part_number"))
                    .toString()
                    .trimmed(),
                query.value(QStringLiteral("id")).toInt());
        }
    }

    if (partIdByNumber.isEmpty()) {
        result.message =
            QStringLiteral("The BrickSuite Parts Catalog is empty. "
                           "Import parts.csv before part_relationships.csv.");
        return result;
    }

    //
    // Existing Rebrickable rows let us classify insert/update/unchanged
    // without querying one relationship at a time.
    //
    QHash<QString, ExistingRelationship> existing;

    {
        QSqlQuery query(database);

        query.prepare(R"(
            SELECT
                id,
                parent_part_id,
                child_part_id,
                relationship_type,
                source_relationship_type,
                is_active
            FROM part_relationship
            WHERE source = :source
        )");

        query.bindValue(":source",
                        QString::fromLatin1(RebrickableSource));

        if (!query.exec()) {
            result.message =
                QStringLiteral("Unable to read existing Part Relationships: %1")
                    .arg(query.lastError().text());

            return result;
        }

        while (query.next()) {
            ExistingRelationship item;
            item.id = query.value(QStringLiteral("id")).toInt();
            item.normalizedType =
                query.value(QStringLiteral("relationship_type"))
                    .toString();
            item.active =
                query.value(QStringLiteral("is_active")).toInt() != 0;

            existing.insert(
                relationshipKey(
                    query.value(QStringLiteral("parent_part_id")).toInt(),
                    query.value(QStringLiteral("child_part_id")).toInt(),
                    query.value(QStringLiteral("source_relationship_type"))
                        .toString()),
                item);
        }
    }

    if (!database.transaction()) {
        result.message =
            QStringLiteral("Unable to begin Part Relationship import transaction.");
        return result;
    }

    const QString now =
        QDateTime::currentDateTimeUtc()
            .toString(Qt::ISODateWithMs);

    QSqlQuery insertQuery(database);

    if (!insertQuery.prepare(R"(
        INSERT INTO part_relationship
        (
            parent_part_id,
            child_part_id,
            relationship_type,
            source_relationship_type,
            source,
            is_active,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :parent_part_id,
            :child_part_id,
            :relationship_type,
            :source_relationship_type,
            :source,
            1,
            :created_utc,
            :modified_utc
        )
    )")) {
        database.rollback();

        result.message =
            QStringLiteral("Unable to prepare Part Relationship insert: %1")
                .arg(insertQuery.lastError().text());

        return result;
    }

    QSqlQuery updateQuery(database);

    if (!updateQuery.prepare(R"(
        UPDATE part_relationship
        SET
            relationship_type = :relationship_type,
            is_active = 1,
            modified_utc = :modified_utc
        WHERE id = :id
    )")) {
        database.rollback();

        result.message =
            QStringLiteral("Unable to prepare Part Relationship update: %1")
                .arg(updateQuery.lastError().text());

        return result;
    }

    QSet<int> seenExistingIds;

    while (!stream.atEnd()) {
        const QString line = stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        ++result.rowsRead;

        bool rowOk = false;
        const QStringList fields =
            parseCsvLine(line, rowOk);

        if (!rowOk || fields.size() != headers.size()) {
            ++result.skippedInvalid;

            qWarning() << "Part Relationship import skipped invalid row."
                       << "Row:" << result.rowsRead
                       << "Reason: Malformed CSV row or unexpected field count."
                       << "ExpectedFields:" << headers.size()
                       << "ActualFields:" << fields.size();

            continue;
        }

        const QString sourceType =
            fields.at(relationTypeIndex)
                .trimmed()
                .toUpper();

        const QString childNumber =
            fields.at(childPartIndex).trimmed();

        const QString parentNumber =
            fields.at(parentPartIndex).trimmed();

        const PartRelationshipType normalizedType =
            relationshipTypeForCode(sourceType);

        if (sourceType.isEmpty()
            || childNumber.isEmpty()
            || parentNumber.isEmpty()
            || normalizedType == PartRelationshipType::Unknown) {
            ++result.skippedInvalid;

            QString reason;

            if (sourceType.isEmpty()) {
                reason = QStringLiteral("Relationship type is missing.");
            } else if (childNumber.isEmpty()) {
                reason = QStringLiteral("Child part number is missing.");
            } else if (parentNumber.isEmpty()) {
                reason = QStringLiteral("Parent part number is missing.");
            } else {
                reason =
                    QStringLiteral("Unsupported relationship type.");
            }

            qWarning() << "Part Relationship import skipped invalid row."
                       << "Row:" << result.rowsRead
                       << "RelType:" << sourceType
                       << "ChildPart:" << childNumber
                       << "ParentPart:" << parentNumber
                       << "Reason:" << reason;

            continue;
        }

        const auto parentIt =
            partIdByNumber.constFind(parentNumber);

        if (parentIt == partIdByNumber.constEnd()) {
            ++result.skippedMissingParent;
            continue;
        }

        const auto childIt =
            partIdByNumber.constFind(childNumber);

        if (childIt == partIdByNumber.constEnd()) {
            ++result.skippedMissingChild;
            continue;
        }

        const int parentPartId = parentIt.value();
        const int childPartId = childIt.value();

        if (parentPartId == childPartId) {
            ++result.skippedInvalid;

            qWarning() << "Part Relationship import skipped invalid row."
                       << "Row:" << result.rowsRead
                       << "RelType:" << sourceType
                       << "ChildPart:" << childNumber
                       << "ParentPart:" << parentNumber
                       << "Reason: Parent and child reference the same part.";

            continue;
        }

        const QString key =
            relationshipKey(parentPartId,
                            childPartId,
                            sourceType);

        const auto existingIt =
            existing.constFind(key);

        const QString normalizedTypeText =
            partRelationshipTypeToString(normalizedType);

        if (existingIt == existing.constEnd()) {
            insertQuery.bindValue(":parent_part_id",
                                  parentPartId);
            insertQuery.bindValue(":child_part_id",
                                  childPartId);
            insertQuery.bindValue(":relationship_type",
                                  normalizedTypeText);
            insertQuery.bindValue(":source_relationship_type",
                                  sourceType);
            insertQuery.bindValue(":source",
                                  QString::fromLatin1(RebrickableSource));
            insertQuery.bindValue(":created_utc", now);
            insertQuery.bindValue(":modified_utc", now);

            if (!insertQuery.exec()) {
                database.rollback();

                result.message =
                    QStringLiteral("Unable to insert relationship %1 -> %2: %3")
                        .arg(childNumber,
                             parentNumber,
                             insertQuery.lastError().text());

                return result;
            }

            ++result.inserted;
            continue;
        }

        seenExistingIds.insert(existingIt->id);

        if (existingIt->active
            && existingIt->normalizedType == normalizedTypeText) {
            ++result.unchanged;
            continue;
        }

        updateQuery.bindValue(":relationship_type",
                              normalizedTypeText);
        updateQuery.bindValue(":modified_utc", now);
        updateQuery.bindValue(":id", existingIt->id);

        if (!updateQuery.exec()) {
            database.rollback();

            result.message =
                QStringLiteral("Unable to update relationship %1 -> %2: %3")
                    .arg(childNumber,
                         parentNumber,
                         updateQuery.lastError().text());

            return result;
        }

        ++result.updated;
    }

    //
    // Provider reference data is synchronized, not deleted. Relationships
    // that were previously imported from Rebrickable but are absent from
    // the current file become inactive.
    //
    QSqlQuery deactivateQuery(database);

    if (!deactivateQuery.prepare(R"(
        UPDATE part_relationship
        SET
            is_active = 0,
            modified_utc = :modified_utc
        WHERE id = :id
          AND is_active = 1
    )")) {
        database.rollback();

        result.message =
            QStringLiteral("Unable to prepare Part Relationship deactivation: %1")
                .arg(deactivateQuery.lastError().text());

        return result;
    }

    for (auto it = existing.constBegin();
         it != existing.constEnd();
         ++it) {
        if (!it->active
            || seenExistingIds.contains(it->id)) {
            continue;
        }

        deactivateQuery.bindValue(":modified_utc", now);
        deactivateQuery.bindValue(":id", it->id);

        if (!deactivateQuery.exec()) {
            database.rollback();

            result.message =
                QStringLiteral("Unable to deactivate obsolete Part Relationship: %1")
                    .arg(deactivateQuery.lastError().text());

            return result;
        }

        if (deactivateQuery.numRowsAffected() > 0)
            ++result.deactivated;
    }

    file.close();

    if (!database.commit()) {
        database.rollback();

        result.message =
            QStringLiteral("Unable to commit Part Relationship import: %1")
                .arg(database.lastError().text());

        return result;
    }

    result.success = true;
    result.message =
        QStringLiteral("Part Relationship import completed successfully.");

    qInfo() << "Part Relationship import completed."
            << "File:" << fileName
            << "RowsRead:" << result.rowsRead
            << "Inserted:" << result.inserted
            << "Updated:" << result.updated
            << "Unchanged:" << result.unchanged
            << "SkippedInvalid:" << result.skippedInvalid
            << "SkippedMissingParent:" << result.skippedMissingParent
            << "SkippedMissingChild:" << result.skippedMissingChild
            << "Deactivated:" << result.deactivated;

    return result;
}

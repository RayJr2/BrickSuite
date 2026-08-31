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
 */

#include "BrickOwlInventoryImporter.h"

#include "../models/ExternalColorMapping.h"
#include "../models/ExternalPartIdentifier.h"
#include "../models/ExternalPartMapping.h"
#include "../models/Part.h"
#include "../models/InventoryRecord.h"
#include "../repositories/ExternalColorMappingRepository.h"
#include "../repositories/ExternalPartIdentifierRepository.h"
#include "../repositories/ExternalPartMappingRepository.h"
#include "../repositories/PartRepository.h"
#include "../repositories/InventoryRecordRepository.h"
#include "../services/parts/PartResolver.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

namespace
{
QString normalizedHeader(QString value)
{
    value = value.trimmed();

    if (!value.isEmpty() && value.front() == QChar(0xFEFF))
        value.remove(0, 1);

    return value.toLower();
}

QString brickLinkColorIdForBrickOwlName(const QString& brickOwlName)
{
    // BrickOwl exports LEGO/marketplace color names rather than BrickLink's
    // numeric COLOR value. Only names that differ from BrickSuite/Rebrickable
    // need a bridge here. The numeric result is then reverse-resolved through
    // BrickSuite's existing external_color_mapping table.
    //
    // Keep this table conservative: an unknown name remains unresolved rather
    // than guessing a visually similar color.
    static const QHash<QString, QString> aliases = {
        { QStringLiteral("medium stone gray"), QStringLiteral("86") },
        { QStringLiteral("dark stone gray"), QStringLiteral("85") },
        { QStringLiteral("transparent"), QStringLiteral("12") },
        { QStringLiteral("transparent orange"), QStringLiteral("98") }
    };

    return aliases.value(brickOwlName.trimmed().toLower());
}

QString conditionForInventoryLookup(const InventoryImportPreviewRow& row,
                                    const InventoryImportOptions& options)
{
    const QString source = row.sourceCondition.trimmed();

    if (source.compare(QStringLiteral("New"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("New");

    if (source.compare(QStringLiteral("Used"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("Used");

    return options.condition.trimmed();
}
}

BrickOwlInventoryImporter::BrickOwlInventoryImporter(QSqlDatabase& database)
    : m_database(database)
{}

QStringList BrickOwlInventoryImporter::parseCsvLine(const QString& line) const
{
    QStringList fields;
    QString currentField;
    bool insideQuotes = false;

    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);

        if (ch == QLatin1Char('"')) {
            if (insideQuotes
                && i + 1 < line.size()
                && line.at(i + 1) == QLatin1Char('"')) {
                currentField.append(QLatin1Char('"'));
                ++i;
            } else {
                insideQuotes = !insideQuotes;
            }
        } else if (ch == QLatin1Char(',') && !insideQuotes) {
            fields.append(currentField);
            currentField.clear();
        } else {
            currentField.append(ch);
        }
    }

    fields.append(currentField);
    return fields;
}

QStringList BrickOwlInventoryImporter::extractPartCandidates(
    const QString& name) const
{
    // BrickOwl order names carry the useful design/part identity in the final
    // parenthesized group, for example:
    //   "... (34103)"
    //   "... (14682 / 65571)"
    static const QRegularExpression finalGroup(
        QStringLiteral(R"(\(([^()]*)\)\s*$)"));

    const QRegularExpressionMatch match = finalGroup.match(name.trimmed());

    if (!match.hasMatch())
        return {};

    QStringList candidates;
    QSet<QString> seen;

    const QStringList raw =
        match.captured(1).split(QLatin1Char('/'), Qt::SkipEmptyParts);

    static const QRegularExpression validToken(
        QStringLiteral(R"(^[A-Za-z0-9._-]+$)"));

    for (QString candidate : raw) {
        candidate = candidate.trimmed();

        if (candidate.isEmpty()
            || !validToken.match(candidate).hasMatch()) {
            continue;
        }

        const QString key = candidate.toLower();

        if (seen.contains(key))
            continue;

        candidates.append(candidate);
        seen.insert(key);
    }

    return candidates;
}

bool BrickOwlInventoryImporter::resolvePart(
    const QStringList& candidates,
    InventoryImportPreviewRow& row) const
{
    if (candidates.isEmpty()) {
        row.errorMessage =
            QStringLiteral("No part number candidate found in BrickOwl Name.");
        return false;
    }

    QSet<int> resolvedIds;
    PartResolver resolver;
    ExternalPartMappingRepository mappingRepository;
    ExternalPartIdentifierRepository identifierRepository;

    for (const QString& candidate : candidates) {
        const PartResolutionResult direct = resolver.resolve(candidate);

        if (direct.hasResolvedPart && direct.part.id() > 0)
            resolvedIds.insert(direct.part.id());

        const QList<ExternalPartMapping> mappings =
            mappingRepository.findByProviderAndExternalId(
                QStringLiteral("BrickLink"),
                candidate);

        for (const ExternalPartMapping& mapping : mappings) {
            if (mapping.partId > 0)
                resolvedIds.insert(mapping.partId);
        }

        const QList<ExternalPartIdentifier> identifiers =
            identifierRepository.findByProviderAndExternalId(
                QStringLiteral("BrickLink"),
                candidate,
                true);

        for (const ExternalPartIdentifier& identifier : identifiers) {
            if (identifier.partId > 0)
                resolvedIds.insert(identifier.partId);
        }
    }

    if (resolvedIds.isEmpty()) {
        row.errorMessage =
            QStringLiteral("Part candidate(s) not found in BrickSuite: %1")
                .arg(candidates.join(QStringLiteral(" / ")));
        return false;
    }

    if (resolvedIds.size() > 1) {
        row.errorMessage =
            QStringLiteral("Multiple BrickSuite parts match candidates %1; "
                           "manual resolution is required.")
                .arg(candidates.join(QStringLiteral(" / ")));
        return false;
    }

    PartRepository partRepository;
    const int resolvedId = *resolvedIds.constBegin();
    const std::optional<Part> part = partRepository.getById(resolvedId);

    if (!part) {
        row.errorMessage =
            QStringLiteral("Resolved BrickSuite part is no longer available.");
        return false;
    }

    row.partId = part->id();
    row.partNumber = part->partNumber();
    row.partName = part->name();
    return true;
}

bool BrickOwlInventoryImporter::resolveColor(
    const QString& brickOwlColorName,
    InventoryImportPreviewRow& row) const
{
    const QString sourceName = brickOwlColorName.trimmed();

    if (sourceName.isEmpty()) {
        row.errorMessage = QStringLiteral("BrickOwl Color Name is empty.");
        return false;
    }

    // First use a safe exact case-insensitive BrickSuite name match. This
    // handles colors whose BrickOwl and Rebrickable names already agree.
    {
        QSqlQuery query(m_database);
        query.prepare(R"(
            SELECT id, name, rebrickable_id
            FROM color
            WHERE name = :name COLLATE NOCASE
            LIMIT 2
        )");
        query.bindValue(":name", sourceName);

        if (!query.exec()) {
            row.errorMessage =
                QStringLiteral("Unable to query BrickSuite colors.");
            return false;
        }

        QList<QVariantList> matches;

        while (query.next()) {
            matches.append({
                query.value(QStringLiteral("id")),
                query.value(QStringLiteral("name")),
                query.value(QStringLiteral("rebrickable_id"))
            });
        }

        if (matches.size() == 1) {
            row.colorId = matches.first().at(0).toInt();
            row.colorName = matches.first().at(1).toString();
            row.rebrickableColorId = matches.first().at(2).toInt();
            return true;
        }

        if (matches.size() > 1) {
            row.errorMessage =
                QStringLiteral("BrickSuite color name is ambiguous: %1")
                    .arg(sourceName);
            return false;
        }
    }

    // BrickOwl uses LEGO-style names for several colors (for example
    // "Medium Stone Gray"). Translate only the name to the corresponding
    // BrickLink numeric color identity, then reuse BrickSuite's existing
    // BrickLink color map in reverse.
    const QString brickLinkId =
        brickLinkColorIdForBrickOwlName(sourceName);

    if (brickLinkId.isEmpty()) {
        row.errorMessage =
            QStringLiteral("Color not resolved: %1")
                .arg(sourceName);
        return false;
    }

    ExternalColorMappingRepository mappingRepository;
    const QList<ExternalColorMapping> mappings =
        mappingRepository.findByProviderAndExternalId(
            QStringLiteral("BrickLink"),
            brickLinkId);

    if (mappings.isEmpty()) {
        row.errorMessage =
            QStringLiteral("No BrickLink color mapping is cached for %1 "
                           "(BrickLink color %2).")
                .arg(sourceName, brickLinkId);
        return false;
    }

    QSet<int> colorIds;

    for (const ExternalColorMapping& mapping : mappings) {
        if (mapping.colorId > 0)
            colorIds.insert(mapping.colorId);
    }

    if (colorIds.size() != 1) {
        row.errorMessage =
            QStringLiteral("BrickLink color %1 maps to multiple BrickSuite colors.")
                .arg(brickLinkId);
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT id, name, rebrickable_id
        FROM color
        WHERE id = :color_id
        LIMIT 1
    )");
    query.bindValue(":color_id", *colorIds.constBegin());

    if (!query.exec() || !query.next()) {
        row.errorMessage =
            QStringLiteral("Mapped BrickSuite color is unavailable.");
        return false;
    }

    row.colorId = query.value(QStringLiteral("id")).toInt();
    row.colorName = query.value(QStringLiteral("name")).toString();
    row.rebrickableColorId =
        query.value(QStringLiteral("rebrickable_id")).toInt();

    return true;
}

bool BrickOwlInventoryImporter::previewOrder(
    const QString& filePath,
    const InventoryImportOptions& options,
    InventoryImportPreview& preview)
{
    preview = {};
    preview.source = InventoryImportSource::BrickOwlOrderCsv;
    preview.sourceFilePath = filePath;
    preview.sourceFileName = QFileInfo(filePath).fileName();
    preview.operation = InventoryCsvOperation::Append;

    if (options.workspaceId <= 0 || options.storageLocationId <= 0) {
        qCritical() << "Invalid BrickOwl inventory preview options.";
        return false;
    }

    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Unable to open BrickOwl order file:"
                    << filePath << file.errorString();
        return false;
    }

    QTextStream stream(&file);

    if (stream.atEnd()) {
        qCritical() << "BrickOwl order file is empty:" << filePath;
        return false;
    }

    const QStringList header = parseCsvLine(stream.readLine());
    QHash<QString, int> headerIndex;

    for (int i = 0; i < header.size(); ++i)
        headerIndex.insert(normalizedHeader(header.at(i)), i);

    const QStringList requiredHeaders = {
        QStringLiteral("order id"),
        QStringLiteral("name"),
        QStringLiteral("type"),
        QStringLiteral("color name"),
        QStringLiteral("boid"),
        QStringLiteral("lot id"),
        QStringLiteral("condition"),
        QStringLiteral("ordered quantity"),
        QStringLiteral("refunded quantity")
    };

    for (const QString& required : requiredHeaders) {
        if (!headerIndex.contains(required)) {
            qCritical() << "Unsupported BrickOwl order CSV. Missing header:"
                        << required;
            return false;
        }
    }

    while (!stream.atEnd()) {
        const QString line = stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        ++preview.rowsProcessed;

        InventoryImportPreviewRow row;
        row.presentInSource = true;

        const QStringList fields = parseCsvLine(line);

        if (fields.size() != header.size()) {
            ++preview.failedRows;
            row.status = QStringLiteral("Error");
            row.errorMessage = QStringLiteral("Invalid CSV column count.");
            preview.rows.append(row);
            continue;
        }

        const auto field = [&](const QString& name) -> QString {
            return fields.at(headerIndex.value(name)).trimmed();
        };

        const QString itemType = field(QStringLiteral("type"));

        row.sourceOrderId = field(QStringLiteral("order id"));
        row.sourceLotId = field(QStringLiteral("lot id"));
        row.sourceBoid = field(QStringLiteral("boid"));
        row.sourceColorName = field(QStringLiteral("color name"));
        row.sourceCondition = field(QStringLiteral("condition"));

        bool orderedValid = false;
        bool refundedValid = false;

        row.sourceOrderedQuantity =
            field(QStringLiteral("ordered quantity")).toInt(&orderedValid);

        const QString refundedText =
            field(QStringLiteral("refunded quantity"));

        row.sourceRefundedQuantity =
            refundedText.isEmpty()
                ? 0
                : refundedText.toInt(&refundedValid);

        if (refundedText.isEmpty())
            refundedValid = true;

        row.sourceQuantity =
            row.sourceOrderedQuantity - row.sourceRefundedQuantity;

        const QString name = field(QStringLiteral("name"));
        const QStringList candidates = extractPartCandidates(name);
        row.sourcePartNumber = candidates.join(QStringLiteral(" / "));

        if (itemType.compare(QStringLiteral("Part"), Qt::CaseInsensitive) != 0) {
            ++preview.failedRows;
            row.status = QStringLiteral("Needs Review");
            row.errorMessage =
                QStringLiteral("BrickOwl row Type is not Part: %1")
                    .arg(itemType);
            preview.rows.append(row);
            continue;
        }

        if (!orderedValid
            || !refundedValid
            || row.sourceOrderedQuantity < 0
            || row.sourceRefundedQuantity < 0
            || row.sourceQuantity <= 0) {
            ++preview.failedRows;
            row.status = QStringLiteral("Needs Review");
            row.errorMessage =
                QStringLiteral("Invalid ordered/refunded quantity.");
            preview.rows.append(row);
            continue;
        }

        const bool partResolved = resolvePart(candidates, row);

        QString partError = row.errorMessage;
        row.errorMessage.clear();

        const bool colorResolved = resolveColor(row.sourceColorName, row);
        const QString colorError = row.errorMessage;
        row.errorMessage.clear();

        if (!partResolved || !colorResolved) {
            ++preview.failedRows;
            row.status = QStringLiteral("Needs Review");

            QStringList errors;

            if (!partResolved)
                errors.append(partError);

            if (!colorResolved)
                errors.append(colorError);

            row.errorMessage = errors.join(QStringLiteral(" "));
            preview.totalSourceQuantity += row.sourceQuantity;
            preview.rows.append(row);
            continue;
        }

        // Show the current quantity for the exact eventual receiving target.
        {
            QSqlQuery query(m_database);
            query.prepare(R"(
                SELECT id, quantity
                FROM inventory_record
                WHERE workspace_id = :workspace_id
                  AND part_id = :part_id
                  AND color_id = :color_id
                  AND storage_location_id = :storage_location_id
                  AND condition = :condition
                  AND ownership_type = :ownership_type
                LIMIT 1
            )");

            query.bindValue(":workspace_id", options.workspaceId);
            query.bindValue(":part_id", row.partId);
            query.bindValue(":color_id", row.colorId);
            query.bindValue(":storage_location_id", options.storageLocationId);
            query.bindValue(":condition",
                            conditionForInventoryLookup(row, options));
            query.bindValue(":ownership_type",
                            options.ownershipType.trimmed());

            if (!query.exec()) {
                ++preview.failedRows;
                row.status = QStringLiteral("Error");
                row.errorMessage =
                    QStringLiteral("Unable to query current inventory.");
                preview.totalSourceQuantity += row.sourceQuantity;
                preview.rows.append(row);
                continue;
            }

            if (query.next()) {
                row.inventoryRecordId =
                    query.value(QStringLiteral("id")).toInt();
                row.currentQuantity =
                    query.value(QStringLiteral("quantity")).toInt();
                row.presentInBrickSuite = row.currentQuantity > 0;
            }
        }

        row.resultingQuantity =
            row.currentQuantity + row.sourceQuantity;
        row.difference = row.sourceQuantity;
        row.status =
            row.currentQuantity > 0
                ? QStringLiteral("Append")
                : QStringLiteral("New Inventory");

        ++preview.validRows;
        preview.totalSourceQuantity += row.sourceQuantity;
        preview.rows.append(row);
    }

    qInfo() << "BrickOwl order preview completed."
            << "File:" << filePath
            << "RowsProcessed:" << preview.rowsProcessed
            << "Resolved:" << preview.validRows
            << "NeedsReview:" << preview.failedRows
            << "NetPieces:" << preview.totalSourceQuantity;

    return true;
}


bool BrickOwlInventoryImporter::importPreview(
    const InventoryImportPreview& preview,
    const InventoryImportOptions& options,
    InventoryImportResult& result)
{
    result = {};

    if (preview.source != InventoryImportSource::BrickOwlOrderCsv
        || options.workspaceId <= 0
        || options.storageLocationId <= 0
        || options.operation != InventoryCsvOperation::Append) {
        qCritical() << "Invalid BrickOwl inventory import request.";
        return false;
    }

    // A row still needing review must never be silently dropped. Skipped rows
    // are the explicit user-approved exception.
    for (const InventoryImportPreviewRow& row : preview.rows) {
        if (row.status == QStringLiteral("Skipped"))
            continue;

        if (row.status == QStringLiteral("Needs Review")
            || row.status == QStringLiteral("Error")
            || row.partId <= 0
            || row.colorId <= 0
            || row.sourceQuantity <= 0) {
            qWarning() << "BrickOwl import blocked by unresolved row."
                       << "SourcePart:" << row.sourcePartNumber
                       << "Status:" << row.status
                       << "Error:" << row.errorMessage;
            return false;
        }
    }

    if (!m_database.transaction()) {
        qCritical() << "Unable to begin BrickOwl receiving transaction:"
                    << m_database.lastError().text();
        return false;
    }

    InventoryRecordRepository inventoryRepository;

    for (const InventoryImportPreviewRow& row : preview.rows) {
        ++result.rowsProcessed;

        if (row.status == QStringLiteral("Skipped"))
            continue;

        InventoryRecord record;
        record.setWorkspaceId(options.workspaceId);
        record.setPartId(row.partId);
        record.setColorId(row.colorId);
        record.setStorageLocationId(options.storageLocationId);

        QString condition = row.sourceCondition.trimmed();

        if (condition.compare(QStringLiteral("New"), Qt::CaseInsensitive) == 0)
            condition = QStringLiteral("New");
        else if (condition.compare(QStringLiteral("Used"), Qt::CaseInsensitive) == 0)
            condition = QStringLiteral("Used");
        else
            condition = options.condition.trimmed();

        record.setCondition(condition);
        record.setOwnershipType(options.ownershipType.trimmed());
        record.setQuantity(row.sourceQuantity);

        const QString referenceId =
            row.sourceOrderId.trimmed().isEmpty()
                ? preview.sourceFileName
                : row.sourceOrderId.trimmed();

        QStringList noteParts;
        noteParts.append(QStringLiteral("BrickOwl received order"));

        if (!row.sourceLotId.trimmed().isEmpty())
            noteParts.append(QStringLiteral("Lot %1").arg(row.sourceLotId.trimmed()));

        if (!row.sourceBoid.trimmed().isEmpty())
            noteParts.append(QStringLiteral("BOID %1").arg(row.sourceBoid.trimmed()));

        if (!row.sourcePartNumber.trimmed().isEmpty())
            noteParts.append(
                QStringLiteral("Source part %1").arg(row.sourcePartNumber.trimmed()));

        if (!inventoryRepository.addOrIncreaseQuantity(
                record,
                QStringLiteral("InventoryImport"),
                QStringLiteral("BrickOwlOrder"),
                referenceId,
                noteParts.join(QStringLiteral("; ")),
                false)) {
            ++result.rowsFailed;

            qCritical() << "Unable to receive BrickOwl inventory row."
                        << "Order:" << referenceId
                        << "PartId:" << row.partId
                        << "ColorId:" << row.colorId
                        << "Quantity:" << row.sourceQuantity;

            m_database.rollback();
            return false;
        }

        ++result.rowsImported;
        result.totalQuantityImported += row.sourceQuantity;
    }

    if (!m_database.commit()) {
        qCritical() << "Unable to commit BrickOwl receiving transaction:"
                    << m_database.lastError().text();
        m_database.rollback();
        return false;
    }

    qInfo() << "BrickOwl order imported."
            << "RowsProcessed:" << result.rowsProcessed
            << "RowsImported:" << result.rowsImported
            << "RowsSkipped:" << (result.rowsProcessed - result.rowsImported)
            << "PiecesImported:" << result.totalQuantityImported;

    return true;
}

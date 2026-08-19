/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "RebrickableInventoryDiffCsvWriter.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

RebrickableInventoryDiffCsvWriter::Result
RebrickableInventoryDiffCsvWriter::write(
    const RebrickableInventoryImportPreview& preview,
    DeltaType deltaType) const
{
    Result result;

    if (preview.operation != InventoryCsvOperation::CompareOnly) {
        result.message =
            QStringLiteral("Rebrickable diff CSV export is available only from Compare Only.");
        return result;
    }

    if (preview.failedRows > 0) {
        result.message =
            QStringLiteral("Resolve comparison errors before exporting a Rebrickable diff CSV.");
        return result;
    }

    QString csv;
    QTextStream stream(&csv);

    stream << "Part,Color,Quantity\n";

    for (const RebrickableInventoryImportPreviewRow& row : preview.rows) {
        if (row.status == QStringLiteral("Error"))
            continue;

        int quantity = 0;

        if (deltaType == DeltaType::Append && row.difference > 0) {
            quantity = row.difference;
        } else if (deltaType == DeltaType::Subtract && row.difference < 0) {
            quantity = -row.difference;
        } else {
            continue;
        }

        if (row.partNumber.trimmed().isEmpty()
            || row.rebrickableColorId < 0
            || quantity <= 0) {
            result.message =
                QStringLiteral("A comparison row is missing valid Rebrickable export data.");
            return result;
        }

        stream << csvField(row.partNumber.trimmed())
               << ','
               << row.rebrickableColorId
               << ','
               << quantity
               << '\n';

        ++result.rows;
        result.pieces += quantity;
    }

    if (result.rows <= 0) {
        result.message =
            deltaType == DeltaType::Append
                ? QStringLiteral("There are no parts to append to Rebrickable.")
                : QStringLiteral("There are no parts to subtract from Rebrickable.");
        return result;
    }

    result.success = true;
    result.csv = csv;
    result.message =
        QStringLiteral("Rebrickable diff CSV generated successfully.");

    return result;
}

QString RebrickableInventoryDiffCsvWriter::suggestedFileName(
    const RebrickableInventoryImportPreview& preview,
    DeltaType deltaType)
{
    QFileInfo sourceInfo(preview.sourceFilePath);

    QString baseName = sourceInfo.completeBaseName().trimmed();

    if (baseName.isEmpty())
        baseName = QStringLiteral("rebrickable_parts_inventory");

    const QRegularExpression duplicateDownloadSuffix(
        R"(\s*\(\d+\)\s*$)");

    baseName.remove(duplicateDownloadSuffix);
    baseName = baseName.trimmed();

    const QString suffix =
        deltaType == DeltaType::Append
            ? QStringLiteral("_append")
            : QStringLiteral("_subtract");

    return baseName + suffix + QStringLiteral(".csv");
}

QString RebrickableInventoryDiffCsvWriter::csvField(
    const QString& value)
{
    QString escaped = value;

    if (escaped.contains(QLatin1Char('"')))
        escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));

    if (escaped.contains(QLatin1Char(','))
        || escaped.contains(QLatin1Char('"'))
        || escaped.contains(QLatin1Char('\n'))
        || escaped.contains(QLatin1Char('\r'))) {
        return QStringLiteral("\"%1\"").arg(escaped);
    }

    return escaped;
}

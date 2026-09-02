#include "RebrickableMinifigPartsImporter.h"

#include "RebrickableCsvInputResolver.h"
#include "../database/DatabaseManager.h"
#include "../services/minifigs/MinifigCompositionReplacementService.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
const QString Provider = QStringLiteral("Rebrickable");
const QString Source = QStringLiteral("Rebrickable Minifig parts CSV/ZIP");

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
        } else if (ch == ',' && !inQuotes) {
            fields.append(field);
            field.clear();
        } else {
            field += ch;
        }
    }
    ok = !inQuotes;
    fields.append(field);
    return fields;
}

bool parseBoolean(const QString& value, bool& result)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == "true" || normalized == "1" || normalized == "yes") {
        result = true;
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no") {
        result = false;
        return true;
    }
    return false;
}

} // namespace

RebrickableMinifigPartsImporter::Result RebrickableMinifigPartsImporter::importFile(
    int minifigCatalogId,
    const QString& fileName)
{
    Result result;
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery identity(database);
    identity.prepare(R"(
        SELECT mei.external_id
        FROM minifig_catalog mc
        JOIN minifig_external_identifier mei ON mei.minifig_catalog_id = mc.id
        WHERE mc.id = :id AND mei.provider = :provider AND mei.is_active = 1
        ORDER BY mei.id LIMIT 1
    )");
    identity.bindValue(":id", minifigCatalogId);
    identity.bindValue(":provider", Provider);
    if (!identity.exec()) {
        result.message = QString("Unable to read the selected Minifig identity: %1")
                             .arg(identity.lastError().text());
        return result;
    }
    if (!identity.next()) {
        result.message = QStringLiteral(
            "The selected Minifig has no active Rebrickable identity.");
        return result;
    }
    const QString selectedExternalId = identity.value(0).toString();

    const QRegularExpression figPattern(
        QStringLiteral("(?<![A-Za-z0-9])fig-[0-9]{6}(?![0-9])"),
                                        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch filenameMatch = figPattern.match(QFileInfo(fileName).fileName());
    if (filenameMatch.hasMatch()
        && filenameMatch.captured().compare(selectedExternalId, Qt::CaseInsensitive) != 0) {
        result.message = QString("The selected file is for %1, not the selected Minifig %2. "
                                 "No changes were saved.")
                             .arg(filenameMatch.captured(), selectedExternalId);
        return result;
    }

    QString expectedCsvName = QFileInfo(fileName).completeBaseName();
    if (!expectedCsvName.endsWith(".csv", Qt::CaseInsensitive))
        expectedCsvName = QStringLiteral("parts.csv");
    QTemporaryDir temporaryDirectory;
    QString csvFileName;
    if (!RebrickableCsvInputResolver::resolve(fileName,
                                              expectedCsvName,
                                              temporaryDirectory,
                                              csvFileName,
                                              result.message)) {
        return result;
    }

    QFile file(csvFileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.message = QStringLiteral("Unable to open the selected Minifig parts CSV.");
        return result;
    }
    QTextStream stream(&file);
    if (stream.atEnd()) {
        result.message = QStringLiteral("The selected Minifig parts CSV is empty.");
        return result;
    }
    QString headerLine = stream.readLine();
    if (!headerLine.isEmpty() && headerLine.front() == QChar(0xFEFF))
        headerLine.remove(0, 1);
    bool headerOk = false;
    const QStringList headers = parseCsvLine(headerLine, headerOk);
    const QStringList requiredHeaders = {"Part", "Color", "Quantity"};
    const bool hasSpare = headers.contains("Is Spare");
    if (!headerOk || (headers != requiredHeaders
                      && headers != QStringList{"Part", "Color", "Quantity", "Is Spare"})) {
        result.message = QStringLiteral(
            "The selected file is not a supported Rebrickable Minifig parts CSV. "
            "Required columns: Part, Color, Quantity. Is Spare is optional.");
        return result;
    }

    QList<MinifigCompositionReplacementService::InputRow> pending;
    int csvLine = 1;
    while (!stream.atEnd()) {
        ++csvLine;
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty())
            continue;
        ++result.rowsRead;
        bool rowOk = false;
        const QStringList fields = parseCsvLine(line, rowOk);
        if (!rowOk || fields.size() != headers.size()) {
            result.message = QString("Malformed CSV row %1. No changes were saved.").arg(csvLine);
            return result;
        }
        const QString partNumber = fields.at(0).trimmed();
        bool colorOk = false;
        const int rebrickableColorId = fields.at(1).trimmed().toInt(&colorOk);
        bool quantityOk = false;
        const int quantity = fields.at(2).trimmed().toInt(&quantityOk);
        bool isSpare = false;
        if (!quantityOk || quantity <= 0) {
            result.message = QString("Invalid quantity at CSV row %1. No changes were saved.")
                                 .arg(csvLine);
            return result;
        }
        if (!colorOk) {
            result.message = QString("Invalid Color at CSV row %1. No changes were saved.")
                                 .arg(csvLine);
            return result;
        }
        if (hasSpare && !parseBoolean(fields.at(3), isSpare)) {
            result.message = QString("Invalid Is Spare value at CSV row %1. "
                                     "No changes were saved.")
                                 .arg(csvLine);
            return result;
        }
        MinifigCompositionReplacementService::InputRow row;
        row.rebrickablePartNumber = partNumber;
        row.rebrickableColorId = rebrickableColorId;
        row.quantity = quantity;
        row.isSpare = isSpare;
        row.context = QString("CSV row %1").arg(csvLine);
        pending.append(row);
    }
    if (pending.isEmpty()) {
        result.message = QStringLiteral("The selected Minifig parts CSV contains no rows.");
        return result;
    }

    MinifigCompositionReplacementService service;
    const auto replacement = service.replace(minifigCatalogId, pending, Provider, Source);
    result.success = replacement.success;
    result.compositionRows = replacement.compositionRows;
    if (!replacement.success) {
        result.message = replacement.message;
        return result;
    }
    result.message = QStringLiteral("Minifig parts import completed successfully.");
    return result;
}

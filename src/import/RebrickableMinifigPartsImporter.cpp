#include "RebrickableMinifigPartsImporter.h"

#include "RebrickableCsvInputResolver.h"
#include "../database/DatabaseManager.h"
#include "../models/MinifigCatalogPart.h"
#include "../repositories/MinifigCatalogPartRepository.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>

#include <algorithm>
#include <limits>
#include <tuple>

namespace {
const QString Provider = QStringLiteral("Rebrickable");
const QString Source = QStringLiteral("Rebrickable Minifig parts list");

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

QString logicalKey(int partId, int colorId, bool isSpare)
{
    return QString("%1|%2|%3").arg(partId).arg(colorId).arg(isSpare ? 1 : 0);
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

    QHash<QString, int> partIds;
    QSqlQuery partsQuery(database);
    if (!partsQuery.exec("SELECT id, rebrickable_part_id FROM part "
                         "WHERE rebrickable_part_id IS NOT NULL")) {
        result.message = QString("Unable to read the Part Catalog: %1")
                             .arg(partsQuery.lastError().text());
        return result;
    }
    while (partsQuery.next()) {
        const QString normalizedId = partsQuery.value(1).toString().toCaseFolded();
        const int partId = partsQuery.value(0).toInt();
        if (partIds.contains(normalizedId) && partIds.value(normalizedId) != partId) {
            result.message = QString("The Part Catalog contains an ambiguous Rebrickable "
                                     "Part identity: %1")
                                 .arg(partsQuery.value(1).toString());
            return result;
        }
        partIds.insert(normalizedId, partId);
    }

    QHash<int, int> colorIds;
    QSqlQuery colorsQuery(database);
    if (!colorsQuery.exec("SELECT id, rebrickable_id FROM color WHERE rebrickable_id IS NOT NULL")) {
        result.message = QString("Unable to read the Color Catalog: %1")
                             .arg(colorsQuery.lastError().text());
        return result;
    }
    while (colorsQuery.next())
        colorIds.insert(colorsQuery.value(1).toInt(), colorsQuery.value(0).toInt());

    QHash<QString, MinifigCatalogPart> pending;
    QStringList unresolved;
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
        const int partId = partIds.value(partNumber.toCaseFolded(), 0);
        const int colorId = colorIds.value(rebrickableColorId, 0);
        if (partId <= 0 || colorId <= 0) {
            QStringList issues;
            if (partId <= 0)
                issues.append(QStringLiteral("unresolved Part"));
            if (colorId <= 0)
                issues.append(QStringLiteral("unresolved Color"));
            unresolved.append(QString("row %1: Part %2, Color %3 (%4)")
                                  .arg(csvLine)
                                  .arg(partNumber)
                                  .arg(rebrickableColorId)
                                  .arg(issues.join(", ")));
            continue;
        }
        const QString key = logicalKey(partId, colorId, isSpare);
        if (pending.contains(key)) {
            const qint64 combined = static_cast<qint64>(pending[key].quantityRequired) + quantity;
            if (combined > std::numeric_limits<int>::max()) {
                result.message = QString("Combined quantity is too large at CSV row %1.").arg(csvLine);
                return result;
            }
            pending[key].quantityRequired = static_cast<int>(combined);
        } else {
            MinifigCatalogPart part;
            part.minifigCatalogId = minifigCatalogId;
            part.partId = partId;
            part.colorId = colorId;
            part.quantityRequired = quantity;
            part.isSpare = isSpare;
            part.provider = Provider;
            part.source = Source;
            pending.insert(key, part);
        }
    }
    if (!unresolved.isEmpty()) {
        result.message = QString("Some Part or Color identities could not be resolved:\n%1\n"
                                 "No changes were saved.")
                             .arg(unresolved.join('\n'));
        return result;
    }
    if (pending.isEmpty()) {
        result.message = QStringLiteral("The selected Minifig parts CSV contains no rows.");
        return result;
    }

    QList<MinifigCatalogPart> composition = pending.values();
    std::sort(composition.begin(), composition.end(), [](const auto& left, const auto& right) {
        return std::tie(left.isSpare, left.partId, left.colorId)
               < std::tie(right.isSpare, right.partId, right.colorId);
    });
    if (!database.transaction()) {
        result.message = QStringLiteral("Unable to begin Minifig parts import transaction.");
        return result;
    }
    MinifigCatalogPartRepository repository;
    QString repositoryError;
    if (!repository.replaceForMinifig(minifigCatalogId, composition, repositoryError)) {
        database.rollback();
        result.message = QString("Unable to replace the Minifig composition: %1. "
                                 "No changes were saved.")
                             .arg(repositoryError);
        return result;
    }
    if (!database.commit()) {
        database.rollback();
        result.message = QString("Unable to commit the Minifig parts import: %1")
                             .arg(database.lastError().text());
        return result;
    }
    result.success = true;
    result.compositionRows = composition.size();
    result.message = QStringLiteral("Minifig parts import completed successfully.");
    return result;
}

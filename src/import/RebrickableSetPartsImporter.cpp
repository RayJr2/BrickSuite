#include "RebrickableSetPartsImporter.h"

#include "RebrickableCsvInputResolver.h"
#include "../database/DatabaseManager.h"
#include "../services/sets/SetCompositionReplacementService.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
QStringList parseCsvLine(const QString& line, bool& ok)
{
    QStringList fields; QString field; bool quoted = false; ok = true;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == '"') {
            if (quoted && i + 1 < line.size() && line.at(i + 1) == '"') { field += '"'; ++i; }
            else quoted = !quoted;
        } else if (ch == ',' && !quoted) { fields << field; field.clear(); }
        else field += ch;
    }
    ok = !quoted; fields << field; return fields;
}
bool parseBoolean(const QString& text, bool& value)
{
    const QString normalized = text.trimmed().toLower();
    if (normalized == "true" || normalized == "1" || normalized == "yes") { value = true; return true; }
    if (normalized == "false" || normalized == "0" || normalized == "no") { value = false; return true; }
    return false;
}
}

RebrickableSetPartsImporter::Result RebrickableSetPartsImporter::importFile(
    int setCatalogId, const QString& fileName)
{
    Result result;
    QSqlQuery setQuery(DatabaseManager::instance().database());
    setQuery.prepare("SELECT set_number FROM set_catalog WHERE id=:id");
    setQuery.bindValue(":id", setCatalogId);
    if (!setQuery.exec()) { result.message = QString("Unable to read the selected Set: %1").arg(setQuery.lastError().text()); return result; }
    if (!setQuery.next()) { result.message = QStringLiteral("The selected Set no longer exists."); return result; }
    const QString selectedSet = setQuery.value(0).toString().trimmed();

    // Rebrickable exports are commonly named rebrickable_parts_<set-number>-<name>.csv.
    // Identifier-free names are accepted, but a recognizable Set number must match.
    const QRegularExpression referencePattern(QStringLiteral("(?<![A-Za-z0-9])(\\d{3,}-\\d+)(?![A-Za-z0-9])"));
    const auto match = referencePattern.match(QFileInfo(fileName).fileName());
    if (match.hasMatch() && match.captured(1).compare(selectedSet, Qt::CaseInsensitive) != 0) {
        result.message = QString("The selected file is for Set %1, not the selected Set %2. No changes were saved.")
                             .arg(match.captured(1), selectedSet);
        return result;
    }
    QString expected = QFileInfo(fileName).completeBaseName();
    if (!expected.endsWith(".csv", Qt::CaseInsensitive)) expected = QStringLiteral("parts.csv");
    QTemporaryDir temporaryDirectory; QString csvName;
    if (!RebrickableCsvInputResolver::resolve(fileName, expected, temporaryDirectory, csvName, result.message)) return result;
    QFile file(csvName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { result.message = QStringLiteral("Unable to open the selected Set parts CSV."); return result; }
    QTextStream stream(&file);
    if (stream.atEnd()) { result.message = QStringLiteral("The selected Set parts CSV is empty."); return result; }
    QString headerLine = stream.readLine();
    if (!headerLine.isEmpty() && headerLine.front() == QChar(0xFEFF)) headerLine.remove(0, 1);
    bool headerOk = false; const QStringList headers = parseCsvLine(headerLine, headerOk);
    const bool hasSpare = headers == QStringList{"Part","Color","Quantity","Is Spare"};
    if (!headerOk || (!hasSpare && headers != QStringList{"Part","Color","Quantity"})) {
        result.message = QStringLiteral("The selected file is not a supported Rebrickable Set parts CSV. Required columns: Part, Color, Quantity. Is Spare is optional.");
        return result;
    }
    QList<SetCompositionReplacementService::InputRow> rows;
    int lineNumber = 1;
    while (!stream.atEnd()) {
        ++lineNumber; const QString line = stream.readLine(); if (line.trimmed().isEmpty()) continue;
        ++result.rowsRead; bool ok = false; const QStringList fields = parseCsvLine(line, ok);
        if (!ok || fields.size() != headers.size()) { result.message = QString("Malformed CSV row %1. No changes were saved.").arg(lineNumber); return result; }
        bool colorOk = false, quantityOk = false;
        const int color = fields.at(1).trimmed().toInt(&colorOk);
        const int quantity = fields.at(2).trimmed().toInt(&quantityOk);
        bool spare = false;
        if (!colorOk || color < 0) { result.message = QString("Invalid Color at CSV row %1. No changes were saved.").arg(lineNumber); return result; }
        if (!quantityOk || quantity <= 0) { result.message = QString("Invalid quantity at CSV row %1. No changes were saved.").arg(lineNumber); return result; }
        if (hasSpare && !parseBoolean(fields.at(3), spare)) { result.message = QString("Invalid Is Spare value at CSV row %1. No changes were saved.").arg(lineNumber); return result; }
        rows << SetCompositionReplacementService::InputRow{fields.at(0).trimmed(), color, quantity, spare,
                                                           QString("CSV row %1").arg(lineNumber)};
    }
    if (rows.isEmpty()) { result.message = QStringLiteral("The selected Set parts CSV contains no rows."); return result; }
    const auto replacement = SetCompositionReplacementService().replace(
        setCatalogId, rows, QStringLiteral("Rebrickable"), QStringLiteral("Rebrickable Set parts CSV/ZIP"));
    result.success = replacement.success; result.compositionRows = replacement.compositionRows;
    result.message = replacement.success ? QStringLiteral("Set parts import completed successfully.") : replacement.message;
    return result;
}

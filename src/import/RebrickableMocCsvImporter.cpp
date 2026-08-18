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

#include "RebrickableMocCsvImporter.h"

#include "../database/DatabaseManager.h"

#include "../models/BuildRequirement.h"
#include "../models/Color.h"
#include "../models/Part.h"

#include "../repositories/BuildRequirementRepository.h"
#include "../repositories/ColorRepository.h"
#include "../repositories/PartRepository.h"

#include <QDebug>
#include <QFile>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
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

struct PendingRequirement
{
    int partId = 0;
    int colorId = 0;
    int quantity = 0;
    bool isSpare = false;
};

QString requirementKey(int partId, int colorId, bool isSpare)
{
    return QString("%1|%2|%3").arg(partId).arg(colorId).arg(isSpare ? 1 : 0);
}

} // namespace

RebrickableMocCsvImporter::Result RebrickableMocCsvImporter::importFile(int buildId,
                                                                        const QString& fileName,
                                                                        bool replaceExisting)
{
    Result result;

    if (buildId <= 0) {
        result.message = "Invalid Build ID.";
        qWarning() << "MOC CSV import rejected: invalid Build ID:" << buildId;

        return result;
    }

    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.message = "Unable to open the selected MOC CSV file.";
        qWarning() << "Unable to open MOC CSV file:" << fileName << file.errorString();

        return result;
    }

    QTextStream stream(&file);

    if (stream.atEnd()) {
        result.message = "The selected MOC CSV file is empty.";

        return result;
    }

    QString headerLine = stream.readLine();

    if (!headerLine.isEmpty() && headerLine.front() == QChar(0xFEFF)) {
        headerLine.remove(0, 1);
    }

    bool headerOk = false;

    const QStringList headers = parseCsvLine(headerLine, headerOk);

    const int partIndex = headers.indexOf("Part");

    const int colorIndex = headers.indexOf("Color");

    const int quantityIndex = headers.indexOf("Quantity");

    const int spareIndex = headers.indexOf("Is Spare");

    if (!headerOk || partIndex < 0 || colorIndex < 0 || quantityIndex < 0) {
        result.message = "The selected file is not a supported "
                         "Rebrickable MOC parts CSV.\n\n"
                         "Required columns:\n"
                         "Part, Color, Quantity\n\n"
                         "Optional column:\n"
                         "Is Spare";

        return result;
    }

    const bool hasSpareColumn = spareIndex >= 0;

    if (!hasSpareColumn) {
        qInfo() << "MOC CSV does not contain an Is Spare column; "
                   "defaulting all imported requirements to non-spare."
                << "File:" << fileName;
    }

    PartRepository partRepository;
    ColorRepository colorRepository;

    //
    // Validate the entire file before making any
    // database changes.
    //
    QHash<QString, PendingRequirement> pendingRequirements;

    int csvLineNumber = 1;

    while (!stream.atEnd()) {
        ++csvLineNumber;

        const QString line = stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        ++result.rowsRead;

        bool rowOk = false;

        const QStringList fields = parseCsvLine(line, rowOk);

        if (!rowOk || fields.size() != headers.size()) {
            result.message = QString("Invalid CSV format at line %1.").arg(csvLineNumber);

            return result;
        }

        const QString partNumber = fields.at(partIndex).trimmed();

        bool colorOk = false;

        const int rebrickableColorId = fields.at(colorIndex).trimmed().toInt(&colorOk);

        bool quantityOk = false;

        const int quantity = fields.at(quantityIndex).trimmed().toInt(&quantityOk);

        bool isSpare = false;

        bool spareOk = true;

        if (hasSpareColumn) {
            spareOk = parseBoolean(fields.at(spareIndex), isSpare);
        }

        if (partNumber.isEmpty() || !colorOk || !quantityOk || quantity <= 0 || !spareOk) {
            result.message = QString("Invalid MOC requirement data "
                                     "at CSV line %1.")
                                 .arg(csvLineNumber);

            return result;
        }

        const std::optional<Part> part = partRepository.getByPartNumber(partNumber);

        if (!part) {
            result.message = QString("Part %1 from CSV line %2 was not "
                                     "found in the BrickSuite Parts Catalog.")
                                 .arg(partNumber)
                                 .arg(csvLineNumber);

            return result;
        }

        const std::optional<Color> color = colorRepository.getByRebrickableId(rebrickableColorId);

        if (!color) {
            result.message = QString("Rebrickable Color ID %1 from "
                                     "CSV line %2 was not found in "
                                     "the BrickSuite Color Catalog.")
                                 .arg(rebrickableColorId)
                                 .arg(csvLineNumber);

            return result;
        }

        const QString key = requirementKey(part->id(), color->id(), isSpare);

        //
        // If Rebrickable ever supplies the same
        // Part/Color/Spare combination more than once,
        // combine it into one Build Requirement.
        //
        if (pendingRequirements.contains(key)) {
            PendingRequirement pending = pendingRequirements.value(key);

            pending.quantity += quantity;

            pendingRequirements.insert(key, pending);
        } else {
            PendingRequirement pending;

            pending.partId = part->id();

            pending.colorId = color->id();

            pending.quantity = quantity;

            pending.isSpare = isSpare;

            pendingRequirements.insert(key, pending);
        }
    }

    file.close();

    if (pendingRequirements.isEmpty()) {
        result.message = "The MOC CSV did not contain any requirements.";

        return result;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.transaction()) {
        result.message = "Unable to begin the MOC import transaction.";
        qCritical() << "Unable to begin MOC import transaction."
                    << "BuildId:" << buildId
                    << "DatabaseError:" << database.lastError().text();

        return result;
    }

    BuildRequirementRepository requirementRepository;

    if (replaceExisting) {
        if (!requirementRepository.removeAllForBuild(buildId)) {
            qCritical() << "MOC import failed removing existing requirements."
                        << "BuildId:" << buildId;
            database.rollback();

            result.message = "Unable to remove the existing "
                             "Build Requirements.";

            return result;
        }
    }

    for (auto iterator = pendingRequirements.constBegin();
         iterator != pendingRequirements.constEnd();
         ++iterator) {
        const PendingRequirement& pending = iterator.value();

        BuildRequirement requirement;

        requirement.setBuildId(buildId);

        requirement.setPartId(pending.partId);

        requirement.setColorId(pending.colorId);

        requirement.setQuantityRequired(pending.quantity);

        requirement.setQuantityPulled(0);

        requirement.setIsSpare(pending.isSpare);

        if (!requirementRepository.create(requirement)) {
            qCritical() << "MOC import failed creating requirement."
                        << "BuildId:" << buildId
                        << "PartId:" << pending.partId
                        << "ColorId:" << pending.colorId
                        << "Quantity:" << pending.quantity
                        << "Spare:" << pending.isSpare;
            database.rollback();

            result.message = "Unable to create one of the "
                             "MOC Build Requirements.\n\n"
                             "No changes were saved.";

            return result;
        }

        ++result.requirementsCreated;

        if (pending.isSpare) {
            result.sparePieces += pending.quantity;
        } else {
            result.regularPieces += pending.quantity;
        }
    }

    if (!database.commit()) {
        qCritical() << "Unable to commit MOC requirements import."
                    << "BuildId:" << buildId
                    << "DatabaseError:" << database.lastError().text();
        database.rollback();

        result.message = "Unable to commit the MOC requirements import.";

        return result;
    }

    result.success = true;

    qInfo() << "MOC CSV import transaction completed."
            << "BuildId:" << buildId
            << "RowsRead:" << result.rowsRead
            << "RequirementsCreated:" << result.requirementsCreated
            << "RegularPieces:" << result.regularPieces
            << "SparePieces:" << result.sparePieces
            << "ReplaceExisting:" << replaceExisting;

    result.message = "MOC requirements imported successfully.";

    return result;
}
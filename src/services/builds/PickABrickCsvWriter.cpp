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

#include "PickABrickCsvWriter.h"

#include "PickABrickExportService.h"

#include <QFile>

PickABrickCsvWriter::Result PickABrickCsvWriter::generate(
    const PickABrickExportProjection& projection)
{
    Result result;
    if (!projection.ready()) {
        result.message = projection.error.isEmpty()
            ? QStringLiteral("Resolve or exclude every included row before exporting.")
            : projection.error;
        return result;
    }

    QByteArray csv("elementId,quantity\r\n");
    for (const PickABrickExportRow& row : projection.rows) {
        if (!PickABrickExportService::isValidElementId(row.elementId) || row.quantity <= 0) {
            result.message = QStringLiteral("The Pick a Brick projection contains an invalid row.");
            return result;
        }
        csv += row.elementId.toLatin1();
        csv += ',';
        csv += QByteArray::number(row.quantity);
        csv += "\r\n";
    }
    result.success = true;
    result.csv = csv;
    result.message = QStringLiteral("LEGO Pick a Brick CSV generated successfully.");
    return result;
}

PickABrickCsvWriter::Result PickABrickCsvWriter::write(
    const QString& fileName, const PickABrickExportProjection& projection)
{
    Result result = generate(projection);
    if (!result.success) return result;
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly) || file.write(result.csv) != result.csv.size()) {
        result.success = false;
        result.message = QStringLiteral("Unable to write:\n\n%1").arg(fileName);
    }
    return result;
}

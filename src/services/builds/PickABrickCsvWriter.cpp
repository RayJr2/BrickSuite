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

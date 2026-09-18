#include "MissingPartsCsvWriter.h"

#include <QFile>
#include <QTextStream>

QString MissingPartsCsvWriter::csvField(const QString& value, bool quoteAlways)
{
    QString escaped = value;
    escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    if (quoteAlways || escaped.contains(',') || escaped.contains('"')
        || escaped.contains('\n') || escaped.contains('\r'))
        return QStringLiteral("\"%1\"").arg(escaped);
    return escaped;
}

MissingPartsCsvWriter::Result MissingPartsCsvWriter::generate(
    const MissingPartsExportProjection& projection)
{
    Result result;
    if (projection.fields.isEmpty()) {
        result.message = QStringLiteral("Select at least one export field.");
        return result;
    }

    QString csv;
    QTextStream stream(&csv);
    stream << QChar(0xFEFF);
    for (int column = 0; column < projection.headers.size(); ++column) {
        if (column) stream << ',';
        stream << csvField(projection.headers.at(column), false);
    }
    stream << '\n';

    for (const QStringList& row : projection.rows) {
        for (int column = 0; column < projection.fields.size(); ++column) {
            if (column) stream << ',';
            const QString field = column < row.size() ? row.at(column) : QString();
            stream << csvField(field, !projection.fields.at(column).numeric);
        }
        stream << '\n';
    }

    result.success = true;
    result.csv = csv;
    result.message = QStringLiteral("Missing Parts CSV generated successfully.");
    return result;
}

MissingPartsCsvWriter::Result MissingPartsCsvWriter::write(
    const QString& fileName, const MissingPartsExportProjection& projection)
{
    Result result = generate(projection);
    if (!result.success) return result;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result.success = false;
        result.message = QStringLiteral("Unable to create:\n\n%1").arg(fileName);
        return result;
    }
    QTextStream stream(&file);
    stream << result.csv;
    if (stream.status() != QTextStream::Ok) {
        result.success = false;
        result.message = QStringLiteral("Unable to write:\n\n%1").arg(fileName);
    }
    return result;
}

#include "InventoryColorAuditCsv.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>

namespace {
const QStringList RequiredColumns = {QStringLiteral("inventory_record_id"),
                                     QStringLiteral("part_number"),
                                     QStringLiteral("stored_color_name"),
                                     QStringLiteral("audit_status")};
const QStringList ReviewColumns = {QStringLiteral("review_status"),
                                   QStringLiteral("reviewed_utc"),
                                   QStringLiteral("review_note")};

QByteArray quoted(const QString& value)
{
    QString escaped = value;
    escaped.replace('"', QStringLiteral("\"\""));
    return ('"' + escaped + '"').toUtf8();
}
}

bool InventoryColorAuditCsv::parse(const QByteArray& data, QVector<QStringList>& records,
                                   QString* errorMessage)
{
    QString text = QString::fromUtf8(data);
    if (!text.isEmpty() && text.front() == QChar::ByteOrderMark) text.removeFirst();
    QStringList row;
    QString field;
    bool quotedField = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (quotedField) {
            if (c == '"') {
                if (i + 1 < text.size() && text.at(i + 1) == '"') {
                    field += '"'; ++i;
                } else quotedField = false;
            } else field += c;
        } else if (c == '"' && field.isEmpty()) {
            quotedField = true;
        } else if (c == ',') {
            row.append(field); field.clear();
        } else if (c == '\r' || c == '\n') {
            row.append(field); field.clear();
            records.append(row); row.clear();
            if (c == '\r' && i + 1 < text.size() && text.at(i + 1) == '\n') ++i;
        } else {
            if (c == '"') {
                if (errorMessage) *errorMessage = QStringLiteral("Unexpected quote in CSV data.");
                return false;
            }
            field += c;
        }
    }
    if (quotedField) {
        if (errorMessage) *errorMessage = QStringLiteral("Unterminated quoted CSV field.");
        return false;
    }
    if (!field.isEmpty() || !row.isEmpty()) { row.append(field); records.append(row); }
    while (!records.isEmpty() && records.back().size() == 1 && records.back().front().isEmpty())
        records.removeLast();
    return true;
}

bool InventoryColorAuditCsv::load(const QString& fileName, QString* errorMessage)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = QStringLiteral("Unable to open the audit CSV: %1").arg(file.errorString());
        return false;
    }
    QVector<QStringList> records;
    if (!parse(file.readAll(), records, errorMessage) || records.isEmpty()) {
        if (records.isEmpty() && errorMessage) *errorMessage = QStringLiteral("The audit CSV is empty.");
        return false;
    }
    const QStringList headers = records.takeFirst();
    QHash<QString, int> columns;
    for (int i = 0; i < headers.size(); ++i) {
        const QString key = headers.at(i).trimmed();
        if (key.isEmpty() || columns.contains(key)) {
            if (errorMessage) *errorMessage = QStringLiteral("The audit CSV contains an empty or duplicate column name.");
            return false;
        }
        columns.insert(key, i);
    }
    for (const QString& required : RequiredColumns) {
        if (!columns.contains(required)) {
            if (errorMessage) *errorMessage = QStringLiteral("The audit CSV is missing required column: %1").arg(required);
            return false;
        }
    }
    QVector<Row> rows;
    for (int i = 0; i < records.size(); ++i) {
        if (records.at(i).size() != headers.size()) {
            if (errorMessage) *errorMessage = QStringLiteral("CSV row %1 has an unexpected column count.").arg(i + 2);
            return false;
        }
        bool idOk = false;
        const int id = records.at(i).at(columns.value(QStringLiteral("inventory_record_id"))).trimmed().toInt(&idOk);
        if (!idOk || id <= 0
            || records.at(i).at(columns.value(QStringLiteral("part_number"))).trimmed().isEmpty()
            || records.at(i).at(columns.value(QStringLiteral("stored_color_name"))).trimmed().isEmpty()
            || records.at(i).at(columns.value(QStringLiteral("audit_status"))).trimmed().isEmpty()) {
            if (errorMessage) *errorMessage = QStringLiteral("CSV row %1 has invalid required audit data.").arg(i + 2);
            return false;
        }
        rows.append({records.at(i)});
    }
    m_fileName = QFileInfo(fileName).canonicalFilePath();
    if (m_fileName.isEmpty()) m_fileName = QFileInfo(fileName).absoluteFilePath();
    m_headers = headers; m_columns = columns; m_rows = rows;
    m_needsOriginalBackup = false;
    for (const QString& review : ReviewColumns) {
        if (!m_columns.contains(review)) {
            m_needsOriginalBackup = true;
            m_columns.insert(review, m_headers.size()); m_headers.append(review);
            for (Row& row : m_rows) row.values.append(QString());
        }
    }
    const int statusColumn = column(QStringLiteral("review_status"));
    for (Row& row : m_rows) {
        if (row.values.at(statusColumn).trimmed().isEmpty()
            && row.values.at(column(QStringLiteral("audit_status"))).trimmed() == QStringLiteral("COLOR_MISMATCH"))
            row.values[statusColumn] = QStringLiteral("PENDING");
    }
    const QFileInfo info(m_fileName);
    m_loadedSize = info.size(); m_loadedModifiedUtc = info.lastModified().toUTC();
    return true;
}

int InventoryColorAuditCsv::column(const QString& name) const { return m_columns.value(name, -1); }

QString InventoryColorAuditCsv::value(int rowIndex, const QString& name) const
{
    const int c = column(name);
    return rowIndex >= 0 && rowIndex < m_rows.size() && c >= 0 ? m_rows.at(rowIndex).values.at(c) : QString();
}

QVector<int> InventoryColorAuditCsv::reviewRows(bool skippedOnly) const
{
    QVector<int> result;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (value(i, QStringLiteral("audit_status")) != QStringLiteral("COLOR_MISMATCH")) continue;
        if (!skippedOnly || value(i, QStringLiteral("review_status")) == QStringLiteral("SKIPPED")) result.append(i);
    }
    return result;
}

int InventoryColorAuditCsv::firstPendingRow() const { return nextPendingRow(-1); }

int InventoryColorAuditCsv::nextPendingRow(int afterRow) const
{
    for (int i = afterRow + 1; i < m_rows.size(); ++i)
        if (value(i, QStringLiteral("audit_status")) == QStringLiteral("COLOR_MISMATCH")
            && value(i, QStringLiteral("review_status")) == QStringLiteral("PENDING")) return i;
    for (int i = 0; i <= afterRow && i < m_rows.size(); ++i)
        if (value(i, QStringLiteral("audit_status")) == QStringLiteral("COLOR_MISMATCH")
            && value(i, QStringLiteral("review_status")) == QStringLiteral("PENDING")) return i;
    return -1;
}

InventoryColorAuditCsv::Statistics InventoryColorAuditCsv::statistics() const
{
    Statistics result;
    for (int i : reviewRows()) {
        ++result.total;
        const QString status = value(i, QStringLiteral("review_status"));
        if (status == QStringLiteral("PENDING")) ++result.pending;
        else if (status == QStringLiteral("FIXED")) ++result.fixed;
        else if (status == QStringLiteral("VERIFIED_OK")) ++result.verifiedOk;
        else if (status == QStringLiteral("SKIPPED")) ++result.skipped;
        else if (status == QStringLiteral("NOT_FOUND")) ++result.notFound;
    }
    return result;
}

QString InventoryColorAuditCsv::originalBackupPath(const QString& fileName)
{
    const QFileInfo info(fileName);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".original.csv"));
}

QByteArray InventoryColorAuditCsv::encode(const QStringList& headers, const QVector<Row>& rows)
{
    QByteArray output;
    auto appendRow = [&output](const QStringList& fields) {
        for (int i = 0; i < fields.size(); ++i) { if (i) output += ','; output += quoted(fields.at(i)); }
        output += "\r\n";
    };
    appendRow(headers);
    for (const Row& row : rows) appendRow(row.values);
    return output;
}

bool InventoryColorAuditCsv::write(QString* errorMessage)
{
    const QFileInfo current(m_fileName);
    if (!current.exists() || current.size() != m_loadedSize
        || current.lastModified().toUTC() != m_loadedModifiedUtc) {
        if (errorMessage) *errorMessage = QStringLiteral("The audit CSV changed outside BrickSuite. Reopen it before saving.");
        return false;
    }
    if (m_needsOriginalBackup) {
        const QString backup = originalBackupPath(m_fileName);
        if (!QFileInfo::exists(backup) && !QFile::copy(m_fileName, backup)) {
            if (errorMessage) *errorMessage = QStringLiteral("Unable to create the original audit safety copy.");
            return false;
        }
    }
    QSaveFile output(m_fileName);
    if (!output.open(QIODevice::WriteOnly) || output.write(encode(m_headers, m_rows)) < 0 || !output.commit()) {
        if (errorMessage) *errorMessage = QStringLiteral("Unable to atomically save audit review progress: %1").arg(output.errorString());
        return false;
    }
    m_needsOriginalBackup = false;
    const QFileInfo updated(m_fileName);
    m_loadedSize = updated.size(); m_loadedModifiedUtc = updated.lastModified().toUTC();
    return true;
}

bool InventoryColorAuditCsv::saveDisposition(int rowIndex, const QString& status,
                                             const QString& note, QString* errorMessage)
{
    static const QStringList Allowed = {QStringLiteral("FIXED"), QStringLiteral("VERIFIED_OK"),
                                        QStringLiteral("SKIPPED"), QStringLiteral("NOT_FOUND")};
    if (rowIndex < 0 || rowIndex >= m_rows.size() || !Allowed.contains(status)) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid audit review disposition.");
        return false;
    }
    if (note.size() > 1024) {
        if (errorMessage) *errorMessage = QStringLiteral("Review note must be 1,024 characters or fewer.");
        return false;
    }
    Row previous = m_rows.at(rowIndex);
    m_rows[rowIndex].values[column(QStringLiteral("review_status"))] = status;
    m_rows[rowIndex].values[column(QStringLiteral("reviewed_utc"))] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_rows[rowIndex].values[column(QStringLiteral("review_note"))] = note;
    if (!write(errorMessage)) { m_rows[rowIndex] = previous; return false; }
    return true;
}

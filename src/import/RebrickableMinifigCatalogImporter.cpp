#include "RebrickableMinifigCatalogImporter.h"

#include "RebrickableCsvInputResolver.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
const QString Provider = QStringLiteral("Rebrickable");
const QString Source = QStringLiteral("Rebrickable minifigs.csv");

struct ImportRow
{
    QString externalId;
    QString name;
    int numberOfParts = 0;
    QString imageUrl;
};

struct ExistingRow
{
    int minifigCatalogId = 0;
    int identifierId = 0;
    QString name;
    int numberOfParts = 0;
    QString imageUrl;
    QString source;
    bool catalogActive = false;
    bool identifierActive = false;
};

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

bool failTransaction(QSqlDatabase& database,
                     RebrickableMinifigCatalogImporter::Result& result,
                     const QString& message)
{
    database.rollback();
    result.message = message;
    return false;
}
} // namespace

RebrickableMinifigCatalogImporter::Result
RebrickableMinifigCatalogImporter::importFile(const QString& fileName)
{
    Result result;
    QTemporaryDir temporaryDirectory;
    QString csvFileName;

    if (!RebrickableCsvInputResolver::resolve(fileName,
                                              QStringLiteral("minifigs.csv"),
                                              temporaryDirectory,
                                              csvFileName,
                                              result.message)) {
        qWarning() << "Minifig Catalog import rejected input:"
                   << fileName << result.message;
        return result;
    }

    QFile file(csvFileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.message = QStringLiteral("Unable to open minifigs.csv.");
        qCritical() << "Minifig Catalog import failed to open file:"
                    << fileName << file.errorString();
        return result;
    }

    QTextStream stream(&file);
    if (stream.atEnd()) {
        result.message = QStringLiteral("minifigs.csv is empty.");
        return result;
    }

    QString header = stream.readLine();
    if (!header.isEmpty() && header.front() == QChar(0xFEFF))
        header.remove(0, 1);

    bool headerOk = false;
    const QStringList headers = parseCsvLine(header, headerOk);
    const QStringList expectedHeaders = {QStringLiteral("fig_num"),
                                         QStringLiteral("name"),
                                         QStringLiteral("num_parts"),
                                         QStringLiteral("img_url")};

    if (!headerOk || headers != expectedHeaders) {
        result.message = QStringLiteral(
            "The selected file is not a supported Rebrickable minifigs.csv file.");
        return result;
    }

    QList<ImportRow> rows;
    QSet<QString> seenExternalIds;

    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty())
            continue;

        ++result.rowsRead;
        bool rowOk = false;
        const QStringList fields = parseCsvLine(line, rowOk);

        if (!rowOk || fields.size() != 4) {
            result.message = QString("Invalid minifigs.csv row at data row %1. "
                                     "No changes were saved.")
                                 .arg(result.rowsRead);
            return result;
        }

        ImportRow row;
        row.externalId = fields.at(0).trimmed();
        row.name = fields.at(1).trimmed();
        row.imageUrl = fields.at(3).trimmed();
        bool partsOk = false;
        row.numberOfParts = fields.at(2).trimmed().toInt(&partsOk);
        const QString normalizedId = row.externalId.toCaseFolded();

        if (row.externalId.isEmpty() || row.name.isEmpty() || !partsOk
            || row.numberOfParts < 0) {
            result.message = QString("Invalid Minifig data at row %1. "
                                     "No changes were saved.")
                                 .arg(result.rowsRead);
            return result;
        }

        if (seenExternalIds.contains(normalizedId)) {
            result.message = QString("Duplicate Rebrickable Minifig ID %1 at row %2. "
                                     "No changes were saved.")
                                 .arg(row.externalId)
                                 .arg(result.rowsRead);
            return result;
        }

        seenExternalIds.insert(normalizedId);
        rows.append(row);
    }

    if (rows.isEmpty()) {
        result.message = QStringLiteral("minifigs.csv contains no catalog rows.");
        return result;
    }

    file.close();

    QSqlDatabase database = DatabaseManager::instance().database();
    QHash<QString, ExistingRow> existingRows;
    {
        QSqlQuery query(database);
        query.prepare(R"(
            SELECT mc.id, mei.id, mei.external_id, mc.name, mc.num_parts,
                   mc.image_url, mei.source, mc.is_active, mei.is_active
            FROM minifig_external_identifier mei
            INNER JOIN minifig_catalog mc ON mc.id = mei.minifig_catalog_id
            WHERE mei.provider = :provider
        )");
        query.bindValue(":provider", Provider);

        if (!query.exec()) {
            result.message = QString("Unable to read the existing Minifig Catalog: %1")
                                 .arg(query.lastError().text());
            return result;
        }

        while (query.next()) {
            ExistingRow existing;
            existing.minifigCatalogId = query.value(0).toInt();
            existing.identifierId = query.value(1).toInt();
            existing.name = query.value(3).toString();
            existing.numberOfParts = query.value(4).toInt();
            existing.imageUrl = query.value(5).toString();
            existing.source = query.value(6).toString();
            existing.catalogActive = query.value(7).toBool();
            existing.identifierActive = query.value(8).toBool();
            existingRows.insert(query.value(2).toString().toCaseFolded(), existing);
        }
    }

    if (!database.transaction()) {
        result.message = QStringLiteral("Unable to begin Minifig Catalog import transaction.");
        return result;
    }

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QSqlQuery insertCatalog(database);
    QSqlQuery insertIdentifier(database);
    QSqlQuery updateCatalog(database);
    QSqlQuery updateIdentifier(database);
    QSqlQuery deactivateIdentifier(database);

    if (!insertCatalog.prepare(R"(
            INSERT INTO minifig_catalog
                (name, num_parts, image_url, is_active, created_utc, modified_utc)
            VALUES
                (:name, :num_parts, :image_url, 1, :created_utc, :modified_utc)
        )")
        || !insertIdentifier.prepare(R"(
            INSERT INTO minifig_external_identifier
                (minifig_catalog_id, provider, external_id, source,
                 is_active, created_utc, modified_utc)
            VALUES
                (:minifig_catalog_id, :provider, :external_id, :source,
                 1, :created_utc, :modified_utc)
        )")
        || !updateCatalog.prepare(R"(
            UPDATE minifig_catalog
            SET name = :name, num_parts = :num_parts, image_url = :image_url,
                is_active = 1, modified_utc = :modified_utc
            WHERE id = :id
        )")
        || !updateIdentifier.prepare(R"(
            UPDATE minifig_external_identifier
            SET source = :source, is_active = 1, modified_utc = :modified_utc
            WHERE id = :id
        )")
        || !deactivateIdentifier.prepare(R"(
            UPDATE minifig_external_identifier
            SET is_active = 0, modified_utc = :modified_utc
            WHERE id = :id AND is_active = 1
        )")) {
        failTransaction(database,
                        result,
                        QStringLiteral("Unable to prepare Minifig Catalog import queries."));
        return result;
    }

    for (const ImportRow& row : rows) {
        const QString key = row.externalId.toCaseFolded();

        if (!existingRows.contains(key)) {
            insertCatalog.bindValue(":name", row.name);
            insertCatalog.bindValue(":num_parts", row.numberOfParts);
            insertCatalog.bindValue(":image_url", row.imageUrl);
            insertCatalog.bindValue(":created_utc", now);
            insertCatalog.bindValue(":modified_utc", now);

            if (!insertCatalog.exec()) {
                failTransaction(database,
                                result,
                                QString("Unable to insert Minifig %1: %2")
                                    .arg(row.externalId, insertCatalog.lastError().text()));
                return result;
            }

            const int minifigCatalogId = insertCatalog.lastInsertId().toInt();
            insertIdentifier.bindValue(":minifig_catalog_id", minifigCatalogId);
            insertIdentifier.bindValue(":provider", Provider);
            insertIdentifier.bindValue(":external_id", row.externalId);
            insertIdentifier.bindValue(":source", Source);
            insertIdentifier.bindValue(":created_utc", now);
            insertIdentifier.bindValue(":modified_utc", now);

            if (!insertIdentifier.exec()) {
                failTransaction(database,
                                result,
                                QString("Unable to store identity for Minifig %1: %2")
                                    .arg(row.externalId, insertIdentifier.lastError().text()));
                return result;
            }

            ++result.inserted;
            continue;
        }

        const ExistingRow existing = existingRows.value(key);
        const bool catalogChanged = existing.name != row.name
                                    || existing.numberOfParts != row.numberOfParts
                                    || existing.imageUrl != row.imageUrl
                                    || !existing.catalogActive;
        const bool identifierChanged = existing.source != Source || !existing.identifierActive;

        if (!catalogChanged && !identifierChanged) {
            ++result.unchanged;
            continue;
        }

        if (catalogChanged) {
            updateCatalog.bindValue(":name", row.name);
            updateCatalog.bindValue(":num_parts", row.numberOfParts);
            updateCatalog.bindValue(":image_url", row.imageUrl);
            updateCatalog.bindValue(":modified_utc", now);
            updateCatalog.bindValue(":id", existing.minifigCatalogId);
            if (!updateCatalog.exec()) {
                failTransaction(database,
                                result,
                                QString("Unable to update Minifig %1: %2")
                                    .arg(row.externalId, updateCatalog.lastError().text()));
                return result;
            }
        }

        if (identifierChanged) {
            updateIdentifier.bindValue(":source", Source);
            updateIdentifier.bindValue(":modified_utc", now);
            updateIdentifier.bindValue(":id", existing.identifierId);
            if (!updateIdentifier.exec()) {
                failTransaction(database,
                                result,
                                QString("Unable to reactivate identity for Minifig %1: %2")
                                    .arg(row.externalId, updateIdentifier.lastError().text()));
                return result;
            }
        }

        ++result.updated;
    }

    for (auto iterator = existingRows.cbegin(); iterator != existingRows.cend(); ++iterator) {
        if (seenExternalIds.contains(iterator.key()) || !iterator.value().identifierActive)
            continue;

        deactivateIdentifier.bindValue(":modified_utc", now);
        deactivateIdentifier.bindValue(":id", iterator.value().identifierId);
        if (!deactivateIdentifier.exec()) {
            failTransaction(database,
                            result,
                            QString("Unable to deactivate a missing Minifig identity: %1")
                                .arg(deactivateIdentifier.lastError().text()));
            return result;
        }
        ++result.deactivated;
    }

    QSqlQuery synchronizeCatalogActivity(database);
    synchronizeCatalogActivity.prepare(R"(
        UPDATE minifig_catalog
        SET is_active = CASE
                WHEN EXISTS (
                    SELECT 1 FROM minifig_external_identifier mei
                    WHERE mei.minifig_catalog_id = minifig_catalog.id
                      AND mei.is_active = 1
                ) THEN 1 ELSE 0 END,
            modified_utc = :modified_utc
        WHERE is_active <> CASE
                WHEN EXISTS (
                    SELECT 1 FROM minifig_external_identifier mei
                    WHERE mei.minifig_catalog_id = minifig_catalog.id
                      AND mei.is_active = 1
                ) THEN 1 ELSE 0 END
    )");
    synchronizeCatalogActivity.bindValue(":modified_utc", now);
    if (!synchronizeCatalogActivity.exec()) {
        failTransaction(database,
                        result,
                        QString("Unable to synchronize Minifig Catalog activity: %1")
                            .arg(synchronizeCatalogActivity.lastError().text()));
        return result;
    }

    if (!database.commit()) {
        database.rollback();
        result.message = QString("Unable to commit Minifig Catalog import: %1")
                             .arg(database.lastError().text());
        return result;
    }

    result.success = true;
    result.message = QStringLiteral("Minifig Catalog import completed successfully.");
    qInfo() << "Minifig Catalog import completed."
            << "File:" << fileName
            << "RowsRead:" << result.rowsRead
            << "Inserted:" << result.inserted
            << "Updated:" << result.updated
            << "Unchanged:" << result.unchanged
            << "Deactivated:" << result.deactivated;
    return result;
}

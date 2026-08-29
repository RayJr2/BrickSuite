#include "ExternalPartIdentifierRepository.h"
#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

bool ExternalPartIdentifierRepository::replaceProviderIds(
    int partId,
    const QHash<QString, QStringList>& externalIds,
    const QString& source) const
{
    if (partId <= 0 || source.trimmed().isEmpty())
        return false;

    QSqlDatabase db = DatabaseManager::instance().database();
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    if (!db.transaction())
        return false;

    QSqlQuery deactivate(db);
    deactivate.prepare(R"(
        UPDATE external_part_identifier
        SET is_active = 0, modified_utc = :modified_utc
        WHERE part_id = :part_id
          AND source = :source
    )");
    deactivate.bindValue(":modified_utc", now);
    deactivate.bindValue(":part_id", partId);
    deactivate.bindValue(":source", source.trimmed());

    if (!deactivate.exec()) {
        db.rollback();
        return false;
    }

    QSqlQuery upsert(db);
    upsert.prepare(R"(
        INSERT INTO external_part_identifier
        (
            part_id, provider, external_id, source,
            is_active, created_utc, modified_utc
        )
        VALUES
        (
            :part_id, :provider, :external_id, :source,
            1, :created_utc, :modified_utc
        )
        ON CONFLICT(part_id, provider, external_id)
        DO UPDATE SET
            source = excluded.source,
            is_active = 1,
            modified_utc = excluded.modified_utc
    )");

    for (auto it = externalIds.constBegin(); it != externalIds.constEnd(); ++it) {
        for (QString id : it.value()) {
            id = id.trimmed();
            if (id.isEmpty())
                continue;

            upsert.bindValue(":part_id", partId);
            upsert.bindValue(":provider", it.key().trimmed());
            upsert.bindValue(":external_id", id);
            upsert.bindValue(":source", source.trimmed());
            upsert.bindValue(":created_utc", now);
            upsert.bindValue(":modified_utc", now);

            if (!upsert.exec()) {
                db.rollback();
                return false;
            }
        }
    }

    return db.commit();
}

QList<ExternalPartIdentifier>
ExternalPartIdentifierRepository::findByExternalId(
    const QString& externalId,
    bool activeOnly) const
{
    QList<ExternalPartIdentifier> results;

    QSqlQuery query(DatabaseManager::instance().database());

    QString sql = R"(
        SELECT id, part_id, provider, external_id, source, is_active
        FROM external_part_identifier
        WHERE external_id = :external_id COLLATE NOCASE
    )";

    if (activeOnly)
        sql += QStringLiteral(" AND is_active = 1");

    sql += QStringLiteral(" ORDER BY part_id, provider");

    query.prepare(sql);
    query.bindValue(":external_id", externalId.trimmed());

    if (!query.exec()) {
        qCritical() << "Unable to search external part identifiers:"
                    << query.lastError().text();
        return results;
    }

    while (query.next()) {
        ExternalPartIdentifier item;
        item.id = query.value("id").toInt();
        item.partId = query.value("part_id").toInt();
        item.provider = query.value("provider").toString();
        item.externalId = query.value("external_id").toString();
        item.source = query.value("source").toString();
        item.isActive = query.value("is_active").toInt() != 0;
        results.append(item);
    }

    return results;
}


QList<ExternalPartIdentifier>
ExternalPartIdentifierRepository::findByProviderAndExternalId(
    const QString& provider,
    const QString& externalId,
    bool activeOnly) const
{
    QList<ExternalPartIdentifier> results;

    const QString providerValue = provider.trimmed();
    const QString externalIdValue = externalId.trimmed();

    if (providerValue.isEmpty() || externalIdValue.isEmpty())
        return results;

    QSqlQuery query(DatabaseManager::instance().database());

    QString sql = R"(
        SELECT id, part_id, provider, external_id, source, is_active
        FROM external_part_identifier
        WHERE provider = :provider COLLATE NOCASE
          AND external_id = :external_id COLLATE NOCASE
    )";

    if (activeOnly)
        sql += QStringLiteral(" AND is_active = 1");

    sql += QStringLiteral(" ORDER BY part_id");

    query.prepare(sql);
    query.bindValue(":provider", providerValue);
    query.bindValue(":external_id", externalIdValue);

    if (!query.exec()) {
        qCritical() << "Unable to search provider external part identifiers:"
                    << query.lastError().text();
        return results;
    }

    while (query.next()) {
        ExternalPartIdentifier item;
        item.id = query.value("id").toInt();
        item.partId = query.value("part_id").toInt();
        item.provider = query.value("provider").toString();
        item.externalId = query.value("external_id").toString();
        item.source = query.value("source").toString();
        item.isActive = query.value("is_active").toInt() != 0;
        results.append(item);
    }

    return results;
}


bool ExternalPartIdentifierRepository::isLookupComplete(
    int partId,
    const QString& source) const
{
    if (partId <= 0 || source.trimmed().isEmpty())
        return false;

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        SELECT status
        FROM external_part_identifier_lookup
        WHERE part_id = :part_id
          AND source = :source COLLATE NOCASE
        LIMIT 1
    )");
    query.bindValue(":part_id", partId);
    query.bindValue(":source", source.trimmed());

    if (!query.exec()) {
        qWarning() << "Unable to read external part identifier lookup status:"
                   << query.lastError().text();
        return false;
    }

    if (!query.next())
        return false;

    const QString status = query.value("status").toString().trimmed();
    return status.compare(QStringLiteral("Loaded"), Qt::CaseInsensitive) == 0
        || status.compare(QStringLiteral("Unavailable"), Qt::CaseInsensitive) == 0;
}

bool ExternalPartIdentifierRepository::setLookupStatus(
    int partId,
    const QString& source,
    const QString& status) const
{
    const QString sourceValue = source.trimmed();
    const QString statusValue = status.trimmed();

    if (partId <= 0 || sourceValue.isEmpty())
        return false;

    if (statusValue.compare(QStringLiteral("Unknown"), Qt::CaseInsensitive) != 0
        && statusValue.compare(QStringLiteral("Loaded"), Qt::CaseInsensitive) != 0
        && statusValue.compare(QStringLiteral("Unavailable"), Qt::CaseInsensitive) != 0) {
        return false;
    }

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        INSERT INTO external_part_identifier_lookup
        (
            part_id, source, status, checked_utc
        )
        VALUES
        (
            :part_id, :source, :status, :checked_utc
        )
        ON CONFLICT(part_id, source)
        DO UPDATE SET
            status = excluded.status,
            checked_utc = excluded.checked_utc
    )");
    query.bindValue(":part_id", partId);
    query.bindValue(":source", sourceValue);
    query.bindValue(":status", statusValue);
    query.bindValue(":checked_utc",
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qWarning() << "Unable to store external part identifier lookup status:"
                   << query.lastError().text();
        return false;
    }

    return true;
}

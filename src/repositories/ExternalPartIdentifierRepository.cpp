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

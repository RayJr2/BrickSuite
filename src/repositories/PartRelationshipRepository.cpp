/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "PartRelationshipRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

namespace
{
PartRelationship relationshipFromQuery(const QSqlQuery& query)
{
    PartRelationship relationship;

    relationship.id = query.value(QStringLiteral("id")).toInt();
    relationship.parentPartId = query.value(QStringLiteral("parent_part_id")).toInt();
    relationship.childPartId = query.value(QStringLiteral("child_part_id")).toInt();
    relationship.relationshipType =
        partRelationshipTypeFromString(
            query.value(QStringLiteral("relationship_type")).toString());
    relationship.sourceRelationshipType =
        query.value(QStringLiteral("source_relationship_type")).toString();
    relationship.source = query.value(QStringLiteral("source")).toString();
    relationship.isActive = query.value(QStringLiteral("is_active")).toInt() != 0;
    relationship.createdUtc = query.value(QStringLiteral("created_utc")).toString();
    relationship.modifiedUtc = query.value(QStringLiteral("modified_utc")).toString();

    return relationship;
}

QList<PartRelationship> executeRelationshipQuery(QSqlQuery& query)
{
    QList<PartRelationship> relationships;

    if (!query.exec()) {
        qCritical() << "Unable to retrieve part relationships:"
                    << query.lastError().text();
        return relationships;
    }

    while (query.next())
        relationships.append(relationshipFromQuery(query));

    return relationships;
}
}

QList<PartRelationship>
PartRelationshipRepository::getByParentPartId(int parentPartId) const
{
    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        SELECT
            id,
            parent_part_id,
            child_part_id,
            relationship_type,
            source_relationship_type,
            source,
            is_active,
            created_utc,
            modified_utc
        FROM part_relationship
        WHERE parent_part_id = :part_id
        ORDER BY relationship_type, child_part_id
    )");

    query.bindValue(":part_id", parentPartId);

    return executeRelationshipQuery(query);
}

QList<PartRelationship>
PartRelationshipRepository::getByChildPartId(int childPartId) const
{
    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        SELECT
            id,
            parent_part_id,
            child_part_id,
            relationship_type,
            source_relationship_type,
            source,
            is_active,
            created_utc,
            modified_utc
        FROM part_relationship
        WHERE child_part_id = :part_id
        ORDER BY relationship_type, parent_part_id
    )");

    query.bindValue(":part_id", childPartId);

    return executeRelationshipQuery(query);
}

QList<int>
PartRelationshipRepository::getActiveDecoratedChildPartIdsByParentPartId(
    int parentPartId,
    int limit) const
{
    QList<int> childPartIds;
    if (parentPartId <= 0 || limit <= 0)
        return childPartIds;

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        SELECT DISTINCT relationship.child_part_id
        FROM part_relationship relationship
        INNER JOIN part child
            ON child.id = relationship.child_part_id
           AND child.is_active = 1
        WHERE relationship.parent_part_id = :part_id
          AND relationship.is_active = 1
          AND relationship.relationship_type IN ('Print', 'Pattern')
        ORDER BY relationship.child_part_id
        LIMIT :limit
    )");
    query.bindValue(QStringLiteral(":part_id"), parentPartId);
    query.bindValue(QStringLiteral(":limit"), limit);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve decorated child Part IDs:"
                    << query.lastError().text();
        return childPartIds;
    }

    while (query.next())
        childPartIds.append(query.value(0).toInt());

    return childPartIds;
}

QList<PartRelationship>
PartRelationshipRepository::getByPartId(int partId) const
{
    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        SELECT
            id,
            parent_part_id,
            child_part_id,
            relationship_type,
            source_relationship_type,
            source,
            is_active,
            created_utc,
            modified_utc
        FROM part_relationship
        WHERE parent_part_id = :part_id
           OR child_part_id = :part_id
        ORDER BY relationship_type, id
    )");

    query.bindValue(":part_id", partId);

    return executeRelationshipQuery(query);
}

bool PartRelationshipRepository::upsert(
    const PartRelationship& relationship) const
{
    if (relationship.parentPartId <= 0
        || relationship.childPartId <= 0
        || relationship.parentPartId == relationship.childPartId
        || relationship.source.trimmed().isEmpty()
        || relationship.sourceRelationshipType.trimmed().isEmpty()) {
        return false;
    }

    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        INSERT INTO part_relationship
        (
            parent_part_id,
            child_part_id,
            relationship_type,
            source_relationship_type,
            source,
            is_active,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :parent_part_id,
            :child_part_id,
            :relationship_type,
            :source_relationship_type,
            :source,
            :is_active,
            :created_utc,
            :modified_utc
        )
        ON CONFLICT(
            parent_part_id,
            child_part_id,
            source_relationship_type,
            source
        )
        DO UPDATE SET
            relationship_type = excluded.relationship_type,
            is_active = excluded.is_active,
            modified_utc = excluded.modified_utc
    )");

    const QString now =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    query.bindValue(":parent_part_id", relationship.parentPartId);
    query.bindValue(":child_part_id", relationship.childPartId);
    query.bindValue(":relationship_type",
                    partRelationshipTypeToString(relationship.relationshipType));
    query.bindValue(":source_relationship_type",
                    relationship.sourceRelationshipType.trimmed());
    query.bindValue(":source", relationship.source.trimmed());
    query.bindValue(":is_active", relationship.isActive ? 1 : 0);
    query.bindValue(":created_utc", now);
    query.bindValue(":modified_utc", now);

    if (!query.exec()) {
        qCritical() << "Unable to save part relationship:"
                    << query.lastError().text();
        return false;
    }

    return true;
}

bool PartRelationshipRepository::setActive(
    int relationshipId,
    bool active) const
{
    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        UPDATE part_relationship
        SET is_active = :is_active,
            modified_utc = :modified_utc
        WHERE id = :id
    )");

    query.bindValue(":is_active", active ? 1 : 0);
    query.bindValue(":modified_utc",
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.bindValue(":id", relationshipId);

    if (!query.exec()) {
        qCritical() << "Unable to update part relationship active state:"
                    << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool PartRelationshipRepository::setAllActiveForSource(
    const QString& source,
    bool active) const
{
    const QString normalizedSource = source.trimmed();

    if (normalizedSource.isEmpty())
        return false;

    QSqlQuery query(DatabaseManager::instance().database());

    query.prepare(R"(
        UPDATE part_relationship
        SET is_active = :is_active,
            modified_utc = :modified_utc
        WHERE source = :source
    )");

    query.bindValue(":is_active", active ? 1 : 0);
    query.bindValue(":modified_utc",
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.bindValue(":source", normalizedSource);

    if (!query.exec()) {
        qCritical() << "Unable to update provider part relationships:"
                    << query.lastError().text();
        return false;
    }

    return true;
}

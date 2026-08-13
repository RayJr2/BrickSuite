#include "BuildRequirementRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

bool BuildRequirementRepository::create(BuildRequirement& requirement)
{
    if (requirement.buildId() <= 0 || requirement.partId() <= 0 || requirement.colorId() <= 0
        || requirement.quantityRequired() <= 0) {
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        INSERT INTO build_requirement
        (
            build_id,
            part_id,
            color_id,
            quantity_required,
            is_spare,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :build_id,
            :part_id,
            :color_id,
            :quantity_required,
            :is_spare,
            :created_utc,
            :modified_utc
        )
    )");

    query.bindValue(":build_id", requirement.buildId());

    query.bindValue(":part_id", requirement.partId());

    query.bindValue(":color_id", requirement.colorId());

    query.bindValue(":quantity_required", requirement.quantityRequired());

    query.bindValue(":is_spare", requirement.isSpare() ? 1 : 0);

    query.bindValue(":created_utc", now.toString(Qt::ISODateWithMs));

    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));

    if (!query.exec()) {
        qCritical() << "Unable to create build requirement:" << query.lastError().text();

        return false;
    }

    requirement.setId(query.lastInsertId().toInt());

    requirement.setCreatedUtc(now);
    requirement.setModifiedUtc(now);

    return true;
}

std::optional<BuildRequirement> BuildRequirementRepository::getById(int id) const
{
    if (id <= 0)
        return std::nullopt;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            build_id,
            part_id,
            color_id,
            quantity_required,
            is_spare,
            created_utc,
            modified_utc
        FROM build_requirement
        WHERE id = :id
    )");

    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve build requirement:" << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return requirementFromQuery(query);
}

QList<BuildRequirement> BuildRequirementRepository::getByBuild(int buildId) const
{
    QList<BuildRequirement> requirements;

    if (buildId <= 0)
        return requirements;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            build_id,
            part_id,
            color_id,
            quantity_required,
            is_spare,
            created_utc,
            modified_utc
        FROM build_requirement
        WHERE build_id = :build_id
        ORDER BY
            part_id,
            color_id,
            is_spare
    )");

    query.bindValue(":build_id", buildId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve build requirements:" << query.lastError().text();

        return requirements;
    }

    while (query.next()) {
        requirements.append(requirementFromQuery(query));
    }

    return requirements;
}

bool BuildRequirementRepository::update(BuildRequirement& requirement)
{
    if (requirement.id() <= 0 || requirement.buildId() <= 0 || requirement.partId() <= 0
        || requirement.colorId() <= 0 || requirement.quantityRequired() <= 0) {
        return false;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery query(database);

    query.prepare(R"(
        UPDATE build_requirement
        SET
            build_id = :build_id,
            part_id = :part_id,
            color_id = :color_id,
            quantity_required = :quantity_required,
            is_spare = :is_spare,
            modified_utc = :modified_utc
        WHERE id = :id
    )");

    query.bindValue(":build_id", requirement.buildId());

    query.bindValue(":part_id", requirement.partId());

    query.bindValue(":color_id", requirement.colorId());

    query.bindValue(":quantity_required", requirement.quantityRequired());

    query.bindValue(":is_spare", requirement.isSpare() ? 1 : 0);

    query.bindValue(":modified_utc", now.toString(Qt::ISODateWithMs));

    query.bindValue(":id", requirement.id());

    if (!query.exec()) {
        qCritical() << "Unable to update build requirement:" << query.lastError().text();

        return false;
    }

    if (query.numRowsAffected() <= 0)
        return false;

    requirement.setModifiedUtc(now);

    return true;
}

bool BuildRequirementRepository::remove(int requirementId)
{
    if (requirementId <= 0)
        return false;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        DELETE FROM build_requirement
        WHERE id = :id
    )");

    query.bindValue(":id", requirementId);

    if (!query.exec()) {
        qCritical() << "Unable to remove build requirement:" << query.lastError().text();

        return false;
    }

    return query.numRowsAffected() > 0;
}

bool BuildRequirementRepository::removeAllForBuild(int buildId)
{
    if (buildId <= 0)
        return false;

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        DELETE FROM build_requirement
        WHERE build_id = :build_id
    )");

    query.bindValue(":build_id", buildId);

    if (!query.exec()) {
        qCritical() << "Unable to remove build requirements:" << query.lastError().text();

        return false;
    }

    //
    // Zero affected rows is still a successful
    // "remove all" operation.
    //
    return true;
}

BuildRequirement BuildRequirementRepository::requirementFromQuery(const QSqlQuery& query) const
{
    BuildRequirement requirement;

    requirement.setId(query.value("id").toInt());

    requirement.setBuildId(query.value("build_id").toInt());

    requirement.setPartId(query.value("part_id").toInt());

    requirement.setColorId(query.value("color_id").toInt());

    requirement.setQuantityRequired(query.value("quantity_required").toInt());

    requirement.setIsSpare(query.value("is_spare").toInt() != 0);

    requirement.setCreatedUtc(
        QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs));

    requirement.setModifiedUtc(
        QDateTime::fromString(query.value("modified_utc").toString(), Qt::ISODateWithMs));

    return requirement;
}
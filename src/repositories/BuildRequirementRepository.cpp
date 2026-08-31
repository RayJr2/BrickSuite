#include "BuildRequirementRepository.h"

#include "../database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool BuildRequirementRepository::create(BuildRequirement& requirement)
{
    if (requirement.buildId() <= 0 || requirement.partId() <= 0 || requirement.colorId() <= 0
        || requirement.quantityRequired() <= 0 || requirement.quantityPulled() < 0
        || requirement.quantityReleased() < 0) {
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
            substitute_part_id,
            substitute_color_id,
            quantity_required,
            quantity_pulled,
            quantity_released,
            is_spare,
            created_utc,
            modified_utc
        )
        VALUES
        (
            :build_id,
            :part_id,
            :color_id,
            :substitute_part_id,
            :substitute_color_id,
            :quantity_required,
            :quantity_pulled,
            :quantity_released,
            :is_spare,
            :created_utc,
            :modified_utc
        )
    )");

    query.bindValue(":build_id", requirement.buildId());
    query.bindValue(":part_id", requirement.partId());
    query.bindValue(":color_id", requirement.colorId());
    query.bindValue(":substitute_part_id",
                    requirement.substitutePartId() > 0 ? QVariant(requirement.substitutePartId()) : QVariant());
    query.bindValue(":substitute_color_id",
                    requirement.substituteColorId() > 0 ? QVariant(requirement.substituteColorId()) : QVariant());
    query.bindValue(":quantity_required", requirement.quantityRequired());
    query.bindValue(":quantity_pulled", requirement.quantityPulled());
    query.bindValue(":quantity_released", requirement.quantityReleased());
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

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        SELECT id, build_id, part_id, color_id,
               substitute_part_id, substitute_color_id,
               quantity_required, quantity_pulled, quantity_released,
               is_spare, created_utc, modified_utc
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

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        SELECT id, build_id, part_id, color_id,
               substitute_part_id, substitute_color_id,
               quantity_required, quantity_pulled, quantity_released,
               is_spare, created_utc, modified_utc
        FROM build_requirement
        WHERE build_id = :build_id
        ORDER BY is_spare, part_id, color_id
    )");
    query.bindValue(":build_id", buildId);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve build requirements:" << query.lastError().text();
        return requirements;
    }
    while (query.next())
        requirements.append(requirementFromQuery(query));
    return requirements;
}

bool BuildRequirementRepository::update(BuildRequirement& requirement)
{
    if (requirement.id() <= 0 || requirement.buildId() <= 0 || requirement.partId() <= 0
        || requirement.colorId() <= 0 || requirement.quantityRequired() <= 0
        || requirement.quantityPulled() < 0 || requirement.quantityReleased() < 0) {
        return false;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        UPDATE build_requirement
        SET build_id = :build_id,
            part_id = :part_id,
            color_id = :color_id,
            substitute_part_id = :substitute_part_id,
            substitute_color_id = :substitute_color_id,
            quantity_required = :quantity_required,
            quantity_pulled = :quantity_pulled,
            quantity_released = :quantity_released,
            is_spare = :is_spare,
            modified_utc = :modified_utc
        WHERE id = :id
    )");
    query.bindValue(":build_id", requirement.buildId());
    query.bindValue(":part_id", requirement.partId());
    query.bindValue(":color_id", requirement.colorId());
    query.bindValue(":substitute_part_id",
                    requirement.substitutePartId() > 0 ? QVariant(requirement.substitutePartId()) : QVariant());
    query.bindValue(":substitute_color_id",
                    requirement.substituteColorId() > 0 ? QVariant(requirement.substituteColorId()) : QVariant());
    query.bindValue(":quantity_required", requirement.quantityRequired());
    query.bindValue(":quantity_pulled", requirement.quantityPulled());
    query.bindValue(":quantity_released", requirement.quantityReleased());
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
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("DELETE FROM build_requirement WHERE id = :id");
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
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("DELETE FROM build_requirement WHERE build_id = :build_id");
    query.bindValue(":build_id", buildId);
    if (!query.exec()) {
        qCritical() << "Unable to remove build requirements:" << query.lastError().text();
        return false;
    }
    return true;
}

std::optional<BuildRequirement> BuildRequirementRepository::getByBuildPartColor(int buildId,
                                                                                int partId,
                                                                                int colorId,
                                                                                bool isSpare) const
{
    if (buildId <= 0 || partId <= 0 || colorId <= 0) {
        return std::nullopt;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT
            id,
            build_id,
            part_id,
            color_id,
            substitute_part_id,
            substitute_color_id,
            quantity_required,
            quantity_pulled,
            quantity_released,
            is_spare,
            created_utc,
            modified_utc
        FROM build_requirement
        WHERE build_id = :build_id
          AND part_id = :part_id
          AND color_id = :color_id
          AND is_spare = :is_spare
        LIMIT 1
    )");

    query.bindValue(":build_id", buildId);
    query.bindValue(":part_id", partId);
    query.bindValue(":color_id", colorId);
    query.bindValue(":is_spare", isSpare ? 1 : 0);

    if (!query.exec()) {
        qCritical() << "Unable to retrieve Build Requirement "
                       "by Build/Part/Color:"
                    << query.lastError().text();

        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return requirementFromQuery(query);
}

BuildRequirement BuildRequirementRepository::requirementFromQuery(const QSqlQuery& query) const
{
    BuildRequirement requirement;
    requirement.setId(query.value("id").toInt());
    requirement.setBuildId(query.value("build_id").toInt());
    requirement.setPartId(query.value("part_id").toInt());
    requirement.setColorId(query.value("color_id").toInt());
    requirement.setSubstitutePartId(query.value("substitute_part_id").toInt());
    requirement.setSubstituteColorId(query.value("substitute_color_id").toInt());
    requirement.setQuantityRequired(query.value("quantity_required").toInt());
    requirement.setQuantityPulled(query.value("quantity_pulled").toInt());
    requirement.setQuantityReleased(query.value("quantity_released").toInt());
    requirement.setIsSpare(query.value("is_spare").toInt() != 0);
    requirement.setCreatedUtc(QDateTime::fromString(query.value("created_utc").toString(), Qt::ISODateWithMs));
    requirement.setModifiedUtc(QDateTime::fromString(query.value("modified_utc").toString(), Qt::ISODateWithMs));
    return requirement;
}

#include "ManufacturerManagementService.h"

#include "../../database/DatabaseManager.h"

#include <QSqlDatabase>
#include <QSqlError>

ManufacturerManagementService::Result ManufacturerManagementService::validateIdentity(
    const Manufacturer& manufacturer, int excludeId) const
{
    if (manufacturer.code().trimmed().isEmpty() || manufacturer.name().trimmed().isEmpty())
        return {false, Error::InvalidInput, "Manufacturer code and name are required."};
    QString databaseError;
    const auto conflict = ManufacturerRepository().identityConflict(
        manufacturer.code(), manufacturer.name(), excludeId, &databaseError);
    if (conflict == ManufacturerIdentityConflict::Code)
        return {false, Error::DuplicateCode, "A Manufacturer with that code already exists."};
    if (conflict == ManufacturerIdentityConflict::Name)
        return {false, Error::DuplicateName, "A Manufacturer with that name already exists."};
    if (conflict == ManufacturerIdentityConflict::DatabaseError)
        return {false, Error::DatabaseFailure, "Unable to validate Manufacturer uniqueness: " + databaseError};
    return {true, Error::None, {}};
}

ManufacturerManagementService::Result ManufacturerManagementService::create(
    Manufacturer manufacturer) const
{
    Result validation = validateIdentity(manufacturer, 0);
    if (!validation.success)
        return validation;
    manufacturer.setOrigin("User");
    manufacturer.setIsActive(true);
    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction())
        return {false, Error::DatabaseFailure, database.lastError().text()};
    if (!ManufacturerRepository().create(manufacturer)) {
        database.rollback();
        return {false, Error::DatabaseFailure, "Unable to create the Manufacturer."};
    }
    if (!database.commit()) {
        database.rollback();
        return {false, Error::DatabaseFailure, database.lastError().text()};
    }
    return {true, Error::None, {}, manufacturer.id()};
}

ManufacturerManagementService::Result ManufacturerManagementService::edit(
    Manufacturer manufacturer) const
{
    ManufacturerRepository repository;
    const auto existing = repository.getById(manufacturer.id());
    if (!existing)
        return {false, Error::NotFound, "The Manufacturer no longer exists."};
    if (existing->origin() != "User")
        return {false, Error::ProtectedOperation, "BrickSuite and provider-owned Manufacturers cannot be edited."};
    Result validation = validateIdentity(manufacturer, manufacturer.id());
    if (!validation.success)
        return validation;
    manufacturer.setOrigin(existing->origin());
    manufacturer.setIsActive(existing->isActive());
    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction())
        return {false, Error::DatabaseFailure, database.lastError().text()};
    if (!repository.update(manufacturer)) {
        database.rollback();
        return {false, Error::DatabaseFailure, "Unable to update the Manufacturer."};
    }
    if (!database.commit()) {
        database.rollback();
        return {false, Error::DatabaseFailure, database.lastError().text()};
    }
    return {true, Error::None, {}, manufacturer.id()};
}

ManufacturerManagementService::Result ManufacturerManagementService::setActive(
    int manufacturerId, bool active) const
{
    ManufacturerRepository repository;
    const auto existing = repository.getById(manufacturerId);
    if (!existing)
        return {false, Error::NotFound, "The Manufacturer no longer exists."};
    if (existing->origin() != "User")
        return {false, Error::ProtectedOperation, "BrickSuite and provider-owned Manufacturers cannot be activated or deactivated manually."};
    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction())
        return {false, Error::DatabaseFailure, database.lastError().text()};
    if (!repository.setActive(manufacturerId, active)) {
        database.rollback();
        return {false, Error::DatabaseFailure, "Unable to update the Manufacturer state."};
    }
    if (!database.commit()) {
        database.rollback();
        return {false, Error::DatabaseFailure, database.lastError().text()};
    }
    return {true, Error::None, {}, manufacturerId};
}

ManufacturerManagementService::UsageResult ManufacturerManagementService::usage(int id) const
{
    if (!ManufacturerRepository().getById(id))
        return {false, "The Manufacturer no longer exists.", {}};
    const ManufacturerUsage value = ManufacturerRepository().usage(id);
    if (!value.success)
        return {false, "Unable to read Manufacturer usage: " + value.errorMessage, value};
    return {true, {}, value};
}

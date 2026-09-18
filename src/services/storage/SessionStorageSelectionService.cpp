#include "SessionStorageSelectionService.h"

#include "../../repositories/StorageLocationRepository.h"

SessionStorageSelectionService::SessionStorageSelectionService(const Validator& validator)
    : m_validator(validator ? validator : [](int workspaceId, int locationId, int excludedId) {
          return StorageLocationRepository().isValidOperationalDestination(
              workspaceId, locationId, excludedId);
      })
{}

int SessionStorageSelectionService::rememberedDestination(int workspaceId,
                                                          int excludedLocationId)
{
    return rememberedDestination(localAuthority(), workspaceId, m_validator,
                                 excludedLocationId);
}

int SessionStorageSelectionService::rememberedDestination(
    const QString& authority, int workspaceId, const Validator& validator,
    int excludedLocationId)
{
    const QString key = authority.trimmed();
    const int locationId = m_destinationByAuthorityAndWorkspace.value(key).value(workspaceId, 0);
    if (workspaceId <= 0 || locationId <= 0
        || key.isEmpty() || !validator
        || !validator(workspaceId, locationId, 0)) {
        if (workspaceId > 0 && !key.isEmpty()) {
            auto authorityIt = m_destinationByAuthorityAndWorkspace.find(key);
            if (authorityIt != m_destinationByAuthorityAndWorkspace.end()) {
                authorityIt->remove(workspaceId);
                if (authorityIt->isEmpty())
                    m_destinationByAuthorityAndWorkspace.erase(authorityIt);
            }
        }
        return 0;
    }
    if (locationId == excludedLocationId)
        return 0;
    return locationId;
}

void SessionStorageSelectionService::rememberDestination(int workspaceId, int locationId)
{
    rememberDestination(localAuthority(), workspaceId, locationId, m_validator);
}

void SessionStorageSelectionService::rememberDestination(
    const QString& authority, int workspaceId, int locationId,
    const Validator& validator)
{
    const QString key = authority.trimmed();
    if (!key.isEmpty() && workspaceId > 0 && locationId > 0 && validator
        && validator(workspaceId, locationId, 0)) {
        m_destinationByAuthorityAndWorkspace[key].insert(workspaceId, locationId);
    }
}

void SessionStorageSelectionService::clearWorkspace(int workspaceId)
{
    for (auto it = m_destinationByAuthorityAndWorkspace.begin();
         it != m_destinationByAuthorityAndWorkspace.end();) {
        it->remove(workspaceId);
        if (it->isEmpty()) it = m_destinationByAuthorityAndWorkspace.erase(it);
        else ++it;
    }
}

void SessionStorageSelectionService::clearAll()
{ m_destinationByAuthorityAndWorkspace.clear(); }

QString SessionStorageSelectionService::localAuthority()
{ return QStringLiteral("local-database"); }

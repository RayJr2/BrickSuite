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
    const int locationId = m_destinationByWorkspace.value(workspaceId, 0);
    if (workspaceId <= 0 || locationId <= 0
        || !m_validator(workspaceId, locationId, 0)) {
        if (workspaceId > 0) m_destinationByWorkspace.remove(workspaceId);
        return 0;
    }
    if (locationId == excludedLocationId)
        return 0;
    return locationId;
}

void SessionStorageSelectionService::rememberDestination(int workspaceId, int locationId)
{
    if (workspaceId > 0 && locationId > 0 && m_validator(workspaceId, locationId, 0))
        m_destinationByWorkspace.insert(workspaceId, locationId);
}

void SessionStorageSelectionService::clearWorkspace(int workspaceId)
{ m_destinationByWorkspace.remove(workspaceId); }

void SessionStorageSelectionService::clearAll()
{ m_destinationByWorkspace.clear(); }

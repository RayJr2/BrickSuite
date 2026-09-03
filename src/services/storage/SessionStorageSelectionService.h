#pragma once

#include <QHash>
#include <functional>

class SessionStorageSelectionService
{
public:
    using Validator = std::function<bool(int workspaceId, int locationId,
                                         int excludedLocationId)>;

    explicit SessionStorageSelectionService(const Validator& validator = {});

    int rememberedDestination(int workspaceId, int excludedLocationId = 0);
    void rememberDestination(int workspaceId, int locationId);
    void clearWorkspace(int workspaceId);
    void clearAll();

private:
    Validator m_validator;
    QHash<int, int> m_destinationByWorkspace;
};

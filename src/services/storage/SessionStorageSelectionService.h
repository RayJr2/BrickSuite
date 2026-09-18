#pragma once

#include <QHash>
#include <QString>
#include <functional>

class SessionStorageSelectionService
{
public:
    using Validator = std::function<bool(int workspaceId, int locationId,
                                         int excludedLocationId)>;

    explicit SessionStorageSelectionService(const Validator& validator = {});

    int rememberedDestination(int workspaceId, int excludedLocationId = 0);
    void rememberDestination(int workspaceId, int locationId);
    int rememberedDestination(const QString& authority, int workspaceId,
                              const Validator& validator,
                              int excludedLocationId = 0);
    void rememberDestination(const QString& authority, int workspaceId,
                             int locationId, const Validator& validator);
    void clearWorkspace(int workspaceId);
    void clearAll();

private:
    Validator m_validator;
    static QString localAuthority();
    QHash<QString, QHash<int, int>> m_destinationByAuthorityAndWorkspace;
};

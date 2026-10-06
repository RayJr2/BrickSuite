/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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

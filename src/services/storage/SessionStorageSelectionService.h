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

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

struct RemoteStorageActionEligibilityInput {
    bool connected=false; bool workspaceCurrent=false; bool stale=true; bool pending=false;
    bool selected=false; bool selectedActive=false;
    bool canGet=false; bool canListTypes=false; bool canAdd=false; bool canEdit=false; bool canSetActive=false;
};
struct RemoteStorageActionEligibilityResult { bool add=false; bool edit=false; bool deactivate=false; bool reactivate=false; };
inline RemoteStorageActionEligibilityResult remoteStorageActionEligibility(const RemoteStorageActionEligibilityInput&i)
{
    const bool ready=i.connected&&i.workspaceCurrent&&!i.stale&&!i.pending;
    return {ready&&i.canAdd&&i.canListTypes,
            ready&&i.selected&&i.canEdit&&i.canGet&&i.canListTypes,
            ready&&i.selected&&i.selectedActive&&i.canSetActive&&i.canGet,
            ready&&i.selected&&!i.selectedActive&&i.canSetActive&&i.canGet};
}

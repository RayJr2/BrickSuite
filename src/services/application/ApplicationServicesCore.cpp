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

#include "ApplicationServices.h"

ApplicationServices::ApplicationServices(std::unique_ptr<WorkspaceApplicationService> w,
    std::unique_ptr<InventoryApplicationService> i, std::unique_ptr<BuildApplicationService> b,
    std::unique_ptr<CollectionApplicationService> c,
    std::unique_ptr<SharedPartReferenceCustomizationService> p, SharedDataSource source)
    : m_workspaces(std::move(w)), m_inventory(std::move(i)), m_builds(std::move(b)),
      m_collection(std::move(c)), m_partReference(std::move(p)), m_source(source) {}

WorkspaceApplicationService& ApplicationServices::workspaces() const { return *m_workspaces; }
InventoryApplicationService& ApplicationServices::inventory() const { return *m_inventory; }
BuildApplicationService& ApplicationServices::builds() const { return *m_builds; }
CollectionApplicationService& ApplicationServices::collection() const { return *m_collection; }
SharedPartReferenceCustomizationService& ApplicationServices::partReferenceCustomizations() const
{ return *m_partReference; }
SharedDataSource ApplicationServices::sharedDataSource() const { return m_source; }
ApplicationServiceStatus ApplicationServices::sharedStatus() const { return m_workspaces->status(); }
void ApplicationServices::setRemoteReads(RemoteReadApplicationServices* remoteReads)
{ m_remoteReads = remoteReads; }
RemoteReadApplicationServices* ApplicationServices::remoteReads() const { return m_remoteReads; }

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

#include "../../models/InventorySearchResult.h"
#include "../../models/export/InventoryExportTypes.h"

#include <QSqlDatabase>

namespace RemoteReadDto { struct InventoryExportRow; }

class InventoryExportService
{
public:
    explicit InventoryExportService(const QSqlDatabase& database = QSqlDatabase());

    QList<InventoryExportRow> createRows(const QList<InventorySearchResult>& source,
                                         const QSet<QString>& enabledFields = {}) const;
    void enrichRows(QList<InventoryExportRow>& rows,
                    const QSet<QString>& enabledFields) const;
    static QList<InventoryExportRow> createRemoteRows(
        const QList<RemoteReadDto::InventoryExportRow>& source);
    static QList<InventoryExportFieldDescriptor> fieldDescriptors();
    static QSet<QString> remoteEnrichmentFieldIds();
    static InventoryExportConfiguration defaultConfiguration();
    static InventoryExportConfiguration normalizeConfiguration(
        const QStringList& order, const QStringList& enabled);
    static InventoryExportProjection project(const QList<InventoryExportRow>& rows,
                                             const InventoryExportConfiguration& configuration,
                                             int maximumRows = -1);

private:
    QSqlDatabase m_database;
};

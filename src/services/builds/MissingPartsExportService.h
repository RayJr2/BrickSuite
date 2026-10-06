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

#include "../../models/export/MissingPartsExportTypes.h"
#include "../application/dto/RemoteReadDtos.h"
#include "MissingPartsService.h"

#include <QSqlDatabase>

class Build;

class MissingPartsExportService
{
public:
    explicit MissingPartsExportService(const QSqlDatabase& database);

    QList<MissingPartsExportRow> createRows(
        const Build& build,
        const QList<MissingPartsService::MissingPart>& missingParts) const;

    static QList<MissingPartsExportRow> createRemoteRows(
        const RemoteReadDto::BuildDetail& build,
        const QList<RemoteReadDto::MissingPart>& missingParts);

    static QList<MissingPartsExportFieldDescriptor> fieldDescriptors();
    static MissingPartsExportConfiguration defaultConfiguration();
    static MissingPartsExportConfiguration normalizeConfiguration(
        const QStringList& savedOrder,
        const QStringList& savedEnabled);
    static MissingPartsExportProjection project(
        const QList<MissingPartsExportRow>& rows,
        const MissingPartsExportConfiguration& configuration);
    static QString value(const MissingPartsExportRow& row,
                         MissingPartsExportField field);

private:
    QSqlDatabase m_database;
};

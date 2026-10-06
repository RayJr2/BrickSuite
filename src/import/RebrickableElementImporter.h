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

#include "global/RebrickableImportCancellation.h"

#include <QString>

class QSqlDatabase;

class RebrickableElementImporter
{
public:
    struct Result {
        bool success=false;
        qint64 rowsRead=0,inserted=0,updated=0,unchanged=0,reactivated=0,deactivated=0;
        QString message;
    };
    Result importFile(const QString& fileName,QSqlDatabase& database,
                      const RebrickableImportCancellation* cancellation=nullptr,
                      const RebrickableRowProgress& progress={}) const;
};

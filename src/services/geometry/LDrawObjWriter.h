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
#include "PartMesh.h"
#include "print/PrintMesh.h"
#include <QString>
class LDrawObjWriter
{
public:
    struct Options {
        double uniformScale = 1.0;
        QString partNumber;
        QString ldrawId;
        QString geometryLabel = QStringLiteral("Source");
    };
    static bool write(const LDrawGeometry::PartMesh& mesh, const QString& path,
                      LDrawGeometry::Error* error = nullptr);
    static bool write(const LDrawGeometry::PartMesh& mesh, const QString& path,
                      const Options& options, LDrawGeometry::Error* error = nullptr);
    static bool write(const PrintGeometry::PrintMesh& mesh, const QString& path,
                      const Options& options, LDrawGeometry::Error* error = nullptr);
};

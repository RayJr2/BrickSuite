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
#include "print/PrintMesh.h"
#include <QColor>
#include <QString>
#include <QVector>
#include <functional>
class ThreeMfWriter { public: struct Options { double uniformScale=1.0; QString objectName; QString partIdentity; QColor modelColor; std::function<void(const QString&)> phase; int collectionDecimalPrecision=-1; }; struct NamedMesh { QString name; PrintGeometry::PrintMesh mesh; PrintGeometry::Point translation; }; static bool write(const PrintGeometry::PrintMesh&,const QString&,const Options&,QString* error=nullptr); static bool writeCollection(const QVector<NamedMesh>&,const QString&,const Options&,QString* error=nullptr); };

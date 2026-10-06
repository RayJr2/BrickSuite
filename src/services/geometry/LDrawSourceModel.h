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
#include <QVector>

#include <array>

namespace LDrawGeometry {

enum class SourceClassification { Unknown, Part, Subpart, Primitive };

struct SourceFileRecord {
    int id = -1;
    QString relativePath;
    SourceClassification classification = SourceClassification::Unknown;
    QString description;
};

struct ReferenceRecord {
    int id = -1;
    int parentId = -1;
    int fileId = -1;
    int sourceLine = 0;
    std::array<double, 12> accumulatedTransform{};
    bool mirrored = false;
    bool inverted = false;
};

struct SurfaceRecord {
    int triangleIndex = -1;
    int referenceId = -1;
    int fileId = -1;
    int sourceLine = 0;
    int sourceType = 0;
    bool certified = false;
    bool clipping = true;
    bool inverted = false;
};

struct LDrawSourceModel {
    QVector<SourceFileRecord> files;
    QVector<ReferenceRecord> references;
    QVector<SurfaceRecord> surfaces;
    QHash<QString, int> fileIds;
};

} // namespace LDrawGeometry

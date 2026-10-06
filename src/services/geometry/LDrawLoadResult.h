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

#include "LDrawSourceModel.h"
#include "PartMesh.h"

#include <QDateTime>
#include <QVector>

#include <memory>

namespace LDrawGeometry {

struct LDrawDependencyRecord {
    QString relativePath;
    qint64 size = 0;
    QDateTime modifiedUtc;
};

struct LDrawDependencyFingerprint {
    QVector<LDrawDependencyRecord> dependencies;
};

struct LDrawLoadResult {
    PartMesh mesh;
    std::shared_ptr<LDrawSourceModel> sourceModel;
    LDrawDependencyFingerprint dependencyFingerprint;
    // External roots have no catalog Part identity; dependencies retain library provenance.
    QString externalFilePath;
    QByteArray externalContentHash;
    Error error;
    bool ok() const { return error.code == ErrorCode::None; }
};

} // namespace LDrawGeometry

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

#include <QVector>
#include <QVector3D>
#include <QString>
#include <QStringList>

namespace LDrawGeometry {

enum class ErrorCode {
    None,
    LibraryNotConfigured,
    InvalidLibrary,
    NoIdentity,
    AmbiguousIdentity,
    ModelNotFound,
    DependencyMissing,
    TraversalRejected,
    CycleDetected,
    ResourceLimitExceeded,
    MalformedSource,
    Cancelled,
    ExportFailed
};

struct Error {
    ErrorCode code = ErrorCode::None;
    QString message;
    QString reference;
    int line = 0;
};

struct Triangle {
    QVector3D a;
    QVector3D b;
    QVector3D c;
    QVector3D normal;
    QString color;
    bool backFaceCull = false;
};

struct Edge {
    QVector3D a;
    QVector3D b;
    QString color;
};

struct ConditionalEdge {
    QVector3D a;
    QVector3D b;
    QVector3D control1;
    QVector3D control2;
    QString color;
};

struct PartMesh {
    QString ldrawId;
    QString sourceRelativePath;
    QString sourceProvenance;
    QVector<Triangle> triangles;
    QVector<Edge> hardEdges;
    QVector<ConditionalEdge> conditionalEdges;
    QVector3D minimumBounds;
    QVector3D maximumBounds;
    bool hasBounds = false;
    bool bfcCertified = false;
    int degenerateFaces = 0;
    int sourceFiles = 0;
    QStringList diagnostics;

    QVector3D dimensionsLdu() const { return maximumBounds - minimumBounds; }
    QVector3D dimensionsMm() const { return dimensionsLdu() * 0.4f; }
};

struct LibraryValidation {
    bool valid = false;
    QString normalizedRoot;
    QString status;
};

} // namespace LDrawGeometry

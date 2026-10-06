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

#include "ManufacturingMeshDiagnosticExporter.h"
#include "../ThreeMfWriter.h"

namespace PrintGeometry {
bool ManufacturingMeshDiagnosticExporter::writeThreeMf(const ManufacturingMesh& mesh, const QString& path,
                                                        double uniformScale, const QColor& modelColor,
                                                        QString* error)
{
    ThreeMfWriter::Options options;
    options.uniformScale = uniformScale;
    options.objectName = QStringLiteral("%1 ManufacturingMesh %2").arg(mesh.partReference, mesh.identity);
    options.partIdentity = mesh.partReference;
    options.modelColor = modelColor;
    return ThreeMfWriter::write(mesh.mesh, path, options, error);
}
}

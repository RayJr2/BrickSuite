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

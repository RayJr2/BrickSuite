#pragma once

#include "ManufacturingMesh.h"
#include <QColor>
#include <QString>

namespace PrintGeometry {
// Deliberately accepts only a ManufacturingMesh so an explicit proof export cannot
// silently substitute Source or nominal Prepared geometry.
class ManufacturingMeshDiagnosticExporter {
public:
    static bool writeThreeMf(const ManufacturingMesh&, const QString& path, double uniformScale,
                             const QColor& modelColor, QString* error = nullptr);
};
}

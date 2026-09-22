#pragma once

#include "../print/PrintMesh.h"
#include <QString>

namespace PrintGeometry {

struct FitFixtureLabelRegion {
    double minimumX = 0;
    double maximumX = 0;
    double minimumY = 0;
    double maximumY = 0;
};

class FitCalibrationFixtureLabel {
public:
    // Cuts lettering into the underside (z = 0) of an already validated flat-base fixture.
    // The caller supplies an explicitly protected base region, not a mating surface.
    static bool recess(const PrintMesh& source, const QString& displayLabel,
                       const QString& shortLabel, const FitFixtureLabelRegion& region,
                       PrintMesh* labeled, QString* appliedLabel,
                       QString* error = nullptr);
};

} // namespace PrintGeometry

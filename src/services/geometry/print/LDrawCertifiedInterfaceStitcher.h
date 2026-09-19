#pragma once

#include "../LDrawLoadResult.h"
#include "LDrawPrintPreparationProfile.h"

#include <QStringList>

namespace PrintGeometry {

struct CertifiedInterfaceStitchDiagnostics {
    int candidateRelationships = 0;
    int acceptedSplits = 0;
    int rejectedAmbiguousCandidates = 0;
    int trianglesBefore = 0;
    int trianglesAfter = 0;
    int boundariesBefore = 0;
    int boundariesAfter = 0;
    QStringList messages;
};

struct CertifiedInterfaceStitchResult {
    LDrawGeometry::LDrawLoadResult loadResult;
    CertifiedInterfaceStitchDiagnostics diagnostics;
    bool changed = false;
};

class LDrawCertifiedInterfaceStitcher
{
public:
    static CertifiedInterfaceStitchResult stitch(
        const LDrawGeometry::LDrawLoadResult& source,
        const LDrawPrintPreparationProfile& profile = {});
};

} // namespace PrintGeometry

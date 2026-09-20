#pragma once

#include "PreparedMesh.h"
#include <QStringList>

namespace PrintGeometry {
struct ManufacturingMesh {
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    QString identity, partReference, nominalPreparationIdentity;
    QString fitProfileIdentity, sourceSessionIdentity, featureIdentity;
    QString semanticContractVersion, correctionContractVersion, regeneratorAlgorithmVersion, booleanVersion;
    double nominalDiameterMillimetres=0, diameterCorrectionMillimetres=0, manufacturingDiameterMillimetres=0;
    QStringList provenance;
};
}

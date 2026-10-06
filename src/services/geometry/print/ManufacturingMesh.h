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

#include "PreparedMesh.h"
#include <QStringList>

namespace PrintGeometry {
// Populated only when a manufacturing route actually applies Verified evidence.
// Nominal/unverified features are deliberately absent from this ledger.
struct AppliedFitFeature {
    FunctionalFeature feature;
    bool nonzero = false;
};
struct ManufacturingMesh {
    PrintMesh mesh;
    MeshAnalysisResult analysis;
    QString identity, partReference, nominalPreparationIdentity;
    QString fitProfileIdentity, sourceSessionIdentity, featureIdentity;
    QStringList featureIdentities;
    QVector<AppliedFitFeature> appliedFitFeatures;
    QString semanticContractVersion, correctionContractVersion, regeneratorAlgorithmVersion, booleanVersion;
    double nominalDiameterMillimetres=0, diameterCorrectionMillimetres=0, manufacturingDiameterMillimetres=0;
    double nominalHeightMillimetres=0, heightCorrectionMillimetres=0, manufacturingHeightMillimetres=0;
    QStringList provenance;
};
}

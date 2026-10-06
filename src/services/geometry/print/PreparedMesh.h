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
#include "PrintMesh.h"
#include "SemanticOperand.h"
#include "SourceCoverage.h"
#include "../LDrawLoadResult.h"
#include <QString>
#include <QStringList>
namespace PrintGeometry {
struct BooleanOperationSummary {int sequence=0;SemanticRole role=SemanticRole::AdditiveAttachment;SemanticFeature feature=SemanticFeature::Stud;QString sourceIdentity;std::size_t sourceTriangles=0,additiveTriangles=0,resultTriangles=0,resultComponents=0;qint64 elapsedMilliseconds=0;bool successful=false;};
struct PrintPreparationTimings {qint64 sourceAnalysisMilliseconds=0,semanticConstructionMilliseconds=0,operandValidationMilliseconds=0,booleanCompositionMilliseconds=0,finalValidationMilliseconds=0,totalMilliseconds=0;};
struct DimensionalFidelityResult {Point sourceDimensions,preparedDimensions,absoluteDimensionDifference;double maximumBoundsDeviationMillimetres=0.0,allowedBoundsDeviationMillimetres=0.0;};
// Validated nominal geometry. Fit compensation and user Scale belong downstream.
struct PreparedMesh { PrintMesh mesh;MeshBounds millimetreBounds;MeshAnalysisResult sourceAnalysis,finalAnalysis;
    std::size_t componentCount=0;QString partReference,ldrawIdentity;
    LDrawGeometry::LDrawDependencyFingerprint dependencyFingerprint;
    QString preparationProfileVersion,mcutVersion,preparationMethod;
    QStringList operationSummary;QVector<BooleanOperationSummary> operations;PrintPreparationTimings timings;
    DimensionalFidelityResult dimensionalFidelity;std::size_t semanticOperandCount=0,sourceTriangleCount=0,preparedTriangleCount=0;
    bool localRepairedOverride=false;
    bool userAcceptedOverride=false;
    QString overrideIdentity;
    QString auditStatus() const { return userAcceptedOverride?QStringLiteral("user_override_success")
        :localRepairedOverride?QStringLiteral("strict_override_success"):QStringLiteral("native_success"); }
    bool hasConformingBodyContacts=false;
    bool hasTopologyAwareLocalComposition=false;
    QVector<FunctionalFeature> functionalFeatures;
    SourceCoverage sourceCoverage;
    QStringList warnings;qint64 elapsedMilliseconds=0; };
}

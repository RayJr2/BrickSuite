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
    QVector<FunctionalFeature> functionalFeatures;
    SourceCoverage sourceCoverage;
    QStringList warnings;qint64 elapsedMilliseconds=0; };
}

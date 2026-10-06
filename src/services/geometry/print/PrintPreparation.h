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
#include "LDrawPrintPreparationProfile.h"
#include "PreparedMesh.h"
#include "SemanticOperand.h"
#include "../LDrawLoadResult.h"
#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>
#include <memory>
namespace PrintGeometry {
class CancellationState {
public:void cancel(){m_cancelled.store(true,std::memory_order_release);}bool isCancelled()const{return m_cancelled.load(std::memory_order_acquire);}
private:std::atomic_bool m_cancelled{false};};
struct PrintPreparationRequest {QString partReference,ldrawIdentity,libraryAuthority;LDrawGeometry::LDrawLoadResult loadResult;LDrawPrintPreparationProfile profile;};
enum class PrintPreparationState {Ready,Unsupported,Ambiguous,Failed,Cancelled};
enum class PrintPreparationError {None,InvalidSource,UnsupportedSemantics,AmbiguousSemantics,OperandClosureFailed,OperandValidationFailed,BooleanFailed,BooleanResultInvalid,FinalValidationFailed,DimensionalFidelityFailed,ResourceLimitExceeded,BackendFailure,Cancelled};
enum class PrintPreparationPhase {SourceAnalysis,SemanticConstruction,OperandValidation,BooleanComposition,FinalValidation,DiagnosticCandidate};
struct PrintPreparationProgress {PrintPreparationPhase phase=PrintPreparationPhase::SourceAnalysis;int currentOperation=0,totalOperations=0;};
using PrintPreparationProgressCallback=std::function<void(const PrintPreparationProgress&)>;
struct PrintPreparationResult {PrintPreparationState state=PrintPreparationState::Failed;PrintPreparationError error=PrintPreparationError::BackendFailure;std::shared_ptr<const PreparedMesh> preparedMesh;std::shared_ptr<const PrintMesh> diagnosticCandidateMesh;MeshAnalysisResult sourceAnalysis,finalAnalysis;QVector<BooleanOperationSummary> operations;PrintPreparationTimings timings;DimensionalFidelityResult dimensionalFidelity;SourceCoverage sourceCoverage;QString preparationProfileIdentity,diagnostic,diagnosticCandidateDescription;QStringList warnings,detailedDiagnostics;std::size_t semanticOperandCount=0;bool cacheHit=false;bool ready()const{return state==PrintPreparationState::Ready&&preparedMesh;}};
}

#include "LDrawPrintPreparationService.h"
#include "CClipBarReceiverSemantic.h"
#include "BallJointSemantic.h"
#include "BallSocketSemantic.h"
#include "StandardStudSourceSemantic.h"
#include "LDrawPrintGeometryBuilder.h"
#include "McutMeshBooleanService.h"
#include "PrintMeshAnalysis.h"
#include "SourceSurfaceSolidifier.h"
#include <QElapsedTimer>
#include <algorithm>
#include <array>
#include <cmath>
namespace PrintGeometry {namespace {
Point dimensions(const MeshBounds&b){return b.valid?Point{b.maximum.x-b.minimum.x,b.maximum.y-b.minimum.y,b.maximum.z-b.minimum.z}:Point{};}
int roleOrder(SemanticRole r){return r==SemanticRole::PrimaryBody?0:r==SemanticRole::SubtractivePassage?1:r==SemanticRole::AdditiveAttachment?2:3;}
QString identity(const SemanticOperand&o){return o.sourceFiles.join('|');}
bool exceedsMeshLimits(const PrintMesh&mesh,const LDrawPrintPreparationProfile&profile){return mesh.vertices.size()>profile.maximumVertices||mesh.faces.size()>profile.maximumFaces;}
bool composeCertifiedBallAssembly(const LDrawGeometry::LDrawLoadResult&loadResult,QVector<SemanticOperand>*operands,QString*diagnostic)
{
    const auto body=std::find_if(operands->cbegin(),operands->cend(),[](const auto&o){return o.role==SemanticRole::PrimaryBody;});
    const auto stem=std::find_if(operands->cbegin(),operands->cend(),[](const auto&o){return o.compositionPriority==1;});
    const auto head=std::find_if(operands->cbegin(),operands->cend(),[](const auto&o){return o.compositionPriority==2;});
    if(stem==operands->cend()&&head==operands->cend())return true;
    if(body==operands->cend()||stem==operands->cend()||head==operands->cend()){*diagnostic="Certified ball assembly is incomplete.";return false;}
    if(std::count_if(operands->cbegin(),operands->cend(),[](const auto&o){return o.compositionPriority==1;})!=1||
       std::count_if(operands->cbegin(),operands->cend(),[](const auto&o){return o.compositionPriority==2;})!=1){
        *diagnostic="Certified ball assembly has ambiguous stem or head ownership.";return false;
    }
    const auto balls=BallJointSemantic::recognize(loadResult);
    if(balls.size()!=1||balls.front().provenance.isEmpty()||!loadResult.sourceModel){*diagnostic="Certified ball ownership is ambiguous.";return false;}
    auto isolated=loadResult;
    isolated.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>(*loadResult.sourceModel);
    isolated.mesh.triangles.clear();isolated.sourceModel->surfaces.clear();
    QVector<int> triangles=stem->sourceTriangleIndices+head->sourceTriangleIndices;
    std::sort(triangles.begin(),triangles.end());
    if(std::adjacent_find(triangles.cbegin(),triangles.cend())!=triangles.cend()){
        *diagnostic="Certified ball assembly has overlapping source-group ownership.";return false;
    }
    for(int triangle:triangles){
        if(triangle<0||triangle>=loadResult.mesh.triangles.size()||triangle>=loadResult.sourceModel->surfaces.size()){
            *diagnostic="Certified ball assembly contains an invalid source-surface index.";return false;
        }
        auto copy=loadResult.sourceModel->surfaces[triangle];copy.triangleIndex=isolated.mesh.triangles.size();
        isolated.mesh.triangles.push_back(loadResult.mesh.triangles[triangle]);
        isolated.sourceModel->surfaces.push_back(copy);
    }
    if(isolated.mesh.triangles.isEmpty()){*diagnostic="Certified ball assembly has no owned source surfaces.";return false;}
    const auto&ball=balls.front();
    const auto axial=[&](const QVector3D&p){return (double(p.x())*.4-ball.frame.origin.x)*ball.frame.axis.x+
        (double(p.z())*.4-ball.frame.origin.y)*ball.frame.axis.y+
        (-double(p.y())*.4-ball.frame.origin.z)*ball.frame.axis.z;};
    double attachmentEnd=-1e100;
    for(int triangle:stem->sourceTriangleIndices){
        const auto&t=loadResult.mesh.triangles[triangle];
        attachmentEnd=std::max({attachmentEnd,axial(t.a),axial(t.b),axial(t.c)});
    }
    if(!std::isfinite(attachmentEnd)){*diagnostic="Certified ball stem has no body-facing boundary.";return false;}
    // Move only the stem's body-facing open ring into the already certified
    // body. This creates actual volumetric overlap instead of relying on a
    // coincident interface; the authoritative source is never modified.
    const double intrusion=.12;
    for(int triangle:stem->sourceTriangleIndices){
        auto& t=isolated.mesh.triangles[std::lower_bound(triangles.cbegin(),triangles.cend(),triangle)-triangles.cbegin()];
        for(auto*point:{&t.a,&t.b,&t.c})if(std::abs(axial(*point)-attachmentEnd)<.02)
            *point+=QVector3D(float(intrusion*ball.frame.axis.x/.4),float(-intrusion*ball.frame.axis.z/.4),float(intrusion*ball.frame.axis.y/.4));
    }
    auto solidified=SourceSurfaceSolidifier::solidify(isolated);
    if(!solidified.successful||!validateBooleanOperand(solidified.analysis).ok()){
        *diagnostic="Certified ball assembly solidification failed: "+solidified.diagnostic;return false;
    }
    SemanticOperand assembly=*stem;
    assembly.closedMesh=std::move(solidified.mesh);assembly.analysis=solidified.analysis;
    assembly.sourceFiles=stem->sourceFiles+head->sourceFiles;
    assembly.sourceTriangleIndices=std::move(triangles);
    const int headIndex=int(head-operands->cbegin()),stemIndex=int(stem-operands->cbegin());
    (*operands)[stemIndex]=std::move(assembly);
    operands->removeAt(headIndex);
    return true;
}
PrintPreparationResult cancelled(const MeshAnalysisResult&a,const QString&profile,const PrintPreparationTimings&t){PrintPreparationResult r;r.state=PrintPreparationState::Cancelled;r.error=PrintPreparationError::Cancelled;r.sourceAnalysis=a;r.preparationProfileIdentity=profile;r.timings=t;r.diagnostic="Print preparation was cancelled.";return r;}
PrintPreparationResult failure(PrintPreparationError e,const MeshAnalysisResult&a,const QString&profile,const QString&message){PrintPreparationResult r;r.error=e;r.sourceAnalysis=a;r.preparationProfileIdentity=profile;r.diagnostic=message;if(e==PrintPreparationError::UnsupportedSemantics)r.state=PrintPreparationState::Unsupported;else if(e==PrintPreparationError::AmbiguousSemantics)r.state=PrintPreparationState::Ambiguous;else r.state=PrintPreparationState::Failed;return r;}
PrintPreparationError mapSemanticError(LDrawSemanticOperandBuilder::Status s){using S=LDrawSemanticOperandBuilder::Status;switch(s){case S::AmbiguousBoundary:return PrintPreparationError::AmbiguousSemantics;case S::UnsupportedLibrarySource:case S::UnsupportedBoundaryTopology:return PrintPreparationError::UnsupportedSemantics;case S::OperandClosureFailed:return PrintPreparationError::OperandClosureFailed;case S::OperandValidationFailed:case S::UncertifiedGeometry:return PrintPreparationError::OperandValidationFailed;case S::ResourceLimitExceeded:return PrintPreparationError::ResourceLimitExceeded;case S::Cancelled:return PrintPreparationError::Cancelled;case S::Ready:return PrintPreparationError::None;}return PrintPreparationError::BackendFailure;}
bool supportsBoundedSourceSolidification(const LDrawGeometry::LDrawLoadResult&loadResult,
                                       const MeshAnalysisResult&source)
{
    if(!source.bounds.valid||source.triangles>2000||source.connectedComponents<2||source.connectedComponents>256||source.boundaryEdges<32||source.boundaryEdges>2048)return false;auto d=dimensions(source.bounds);std::array<double,3>sorted{d.x,d.y,d.z};std::sort(sorted.begin(),sorted.end());
    if(sorted[0]<=6.5&&sorted[1]<=6.5&&sorted[2]<=20.0)return true;
    // A single certified C-Clip on a small integrated part can be solidified
    // without changing the wider source-surface policy for arbitrary plates.
    const bool smallCertifiedInterface=source.triangles<=500 &&
        (CClipBarReceiverSemantic::recognize(loadResult).size()==1 ||
         BallJointSemantic::recognize(loadResult).size()==1);
    const bool ballSocket=source.triangles<=1200 &&
        BallSocketSemantic::recognize(loadResult).size()==1;
    if(ballSocket)return sorted[0]<=12.0 && sorted[1]<=16.0 && sorted[2]<=32.0;
    return sorted[0]<=12.0 && sorted[1]<=16.0 && sorted[2]<=24.0 && smallCertifiedInterface;
}
PrintPreparationResult solidifiedResult(const PrintPreparationRequest&request,const MeshAnalysisResult&sourceAnalysis,SourceSurfaceSolidificationResult solidified,const PrintPreparationTimings&partialTimings,qint64 elapsed)
{
    PrintPreparationResult result;result.sourceAnalysis=sourceAnalysis;result.finalAnalysis=solidified.analysis;result.preparationProfileIdentity=request.profile.identity;result.dimensionalFidelity.sourceDimensions=dimensions(sourceAnalysis.bounds);result.dimensionalFidelity.preparedDimensions=dimensions(solidified.analysis.bounds);result.dimensionalFidelity.absoluteDimensionDifference={std::abs(result.dimensionalFidelity.sourceDimensions.x-result.dimensionalFidelity.preparedDimensions.x),std::abs(result.dimensionalFidelity.sourceDimensions.y-result.dimensionalFidelity.preparedDimensions.y),std::abs(result.dimensionalFidelity.sourceDimensions.z-result.dimensionalFidelity.preparedDimensions.z)};result.dimensionalFidelity.maximumBoundsDeviationMillimetres=maximumBoundsDeviation(sourceAnalysis.bounds,solidified.analysis.bounds);result.dimensionalFidelity.allowedBoundsDeviationMillimetres=request.profile.maximumExternalBoundsDeviationMillimetres;
    if(result.dimensionalFidelity.maximumBoundsDeviationMillimetres>request.profile.maximumExternalBoundsDeviationMillimetres)return failure(PrintPreparationError::DimensionalFidelityFailed,sourceAnalysis,request.profile.identity,"Solidified source exceeded the external-bounds preservation limit.");
    auto prepared=std::make_shared<PreparedMesh>();prepared->mesh=std::move(solidified.mesh);prepared->millimetreBounds=solidified.analysis.bounds;prepared->sourceAnalysis=sourceAnalysis;prepared->finalAnalysis=solidified.analysis;prepared->componentCount=solidified.analysis.connectedComponents;prepared->partReference=request.partReference;prepared->ldrawIdentity=request.ldrawIdentity;prepared->dependencyFingerprint=request.loadResult.dependencyFingerprint;prepared->preparationProfileVersion=request.profile.identity+QStringLiteral("+source-surface-solidifier-v1");prepared->preparationMethod=QStringLiteral("Certified authoritative LDraw source-surface volumetric solidification");prepared->sourceTriangleCount=sourceAnalysis.triangles;prepared->preparedTriangleCount=solidified.analysis.triangles;prepared->elapsedMilliseconds=elapsed;prepared->dimensionalFidelity=result.dimensionalFidelity;prepared->warnings<<QStringLiteral("Source-surface solidification used a %1 mm sampling pitch; inspect fine functional details before printing.").arg(solidified.samplingPitchMillimetres,0,'f',3);prepared->timings=partialTimings;prepared->timings.totalMilliseconds=elapsed;prepared->functionalFeatures=CClipBarReceiverSemantic::recognize(request.loadResult);prepared->functionalFeatures+=BallJointSemantic::recognize(request.loadResult);prepared->functionalFeatures+=BallSocketSemantic::recognize(request.loadResult);
    for(const auto& stud:certifiedSourceStuds(request.loadResult))
        prepared->functionalFeatures.push_back(stud.feature);
    result.state=PrintPreparationState::Ready;result.error=PrintPreparationError::None;result.preparedMesh=prepared;result.timings=prepared->timings;result.warnings=prepared->warnings;result.diagnostic=solidified.diagnostic+QStringLiteral(" Independently validated as one closed printable solid.");return result;
}
}
LDrawPrintPreparationService::LDrawPrintPreparationService(std::shared_ptr<PrintPreparationCache>cache,BooleanServiceFactory factory,SemanticBuilderFunction semanticBuilder):m_cache(cache?std::move(cache):std::make_shared<PrintPreparationCache>()),m_factory(factory?std::move(factory):[]{return std::make_unique<McutMeshBooleanService>();}),m_semanticBuilder(std::move(semanticBuilder)){}
PrintPreparationResult LDrawPrintPreparationService::prepare(const PrintPreparationRequest&request,const CancellationState*cancellation,const PrintPreparationProgressCallback&progress)const
{
    QElapsedTimer total,phase;total.start();PrintPreparationTimings timings;MeshAnalysisResult sourceAnalysis;
    if(cancellation&&cancellation->isCancelled())return cancelled(sourceAnalysis,request.profile.identity,timings);
    auto backend=m_factory();if(!backend)return failure(PrintPreparationError::BackendFailure,sourceAnalysis,request.profile.identity,"No Boolean service is available.");
    const auto key=PrintPreparationCache::keyFor(request,backend->versionIdentity());
    if(auto cached=m_cache->find(key)){PrintPreparationResult hit;hit.state=PrintPreparationState::Ready;hit.error=PrintPreparationError::None;hit.preparedMesh=std::move(cached);hit.sourceAnalysis=hit.preparedMesh->sourceAnalysis;hit.finalAnalysis=hit.preparedMesh->finalAnalysis;hit.operations=hit.preparedMesh->operations;hit.timings=hit.preparedMesh->timings;hit.dimensionalFidelity=hit.preparedMesh->dimensionalFidelity;hit.semanticOperandCount=hit.preparedMesh->semanticOperandCount;hit.preparationProfileIdentity=request.profile.identity;hit.warnings=hit.preparedMesh->warnings;hit.cacheHit=true;hit.diagnostic="Prepared geometry returned from the session cache.";hit.timings.totalMilliseconds=total.elapsed();return hit;}
    if(!request.loadResult.ok()||!request.loadResult.sourceModel)return failure(PrintPreparationError::InvalidSource,sourceAnalysis,request.profile.identity,"The authoritative LDraw load result is invalid.");
    if(std::size_t(request.loadResult.mesh.triangles.size())>request.profile.maximumFaces)return failure(PrintPreparationError::ResourceLimitExceeded,sourceAnalysis,request.profile.identity,"Source triangle limit exceeded.");
    for(const auto&triangle:request.loadResult.mesh.triangles)for(const auto&point:{triangle.a,triangle.b,triangle.c})if(!qIsFinite(point.x())||!qIsFinite(point.y())||!qIsFinite(point.z()))return failure(PrintPreparationError::InvalidSource,sourceAnalysis,request.profile.identity,"Source geometry contains non-finite coordinates.");
    try{
        if(progress)progress({PrintPreparationPhase::SourceAnalysis,0,0});
        phase.start();auto semantic=m_semanticBuilder?m_semanticBuilder(request.loadResult):LDrawSemanticOperandBuilder::build(request.loadResult,[cancellation]{return cancellation&&cancellation->isCancelled();});sourceAnalysis=semantic.sourceAnalysis;timings.sourceAnalysisMilliseconds=semantic.sourceConversionAnalysisMs;timings.semanticConstructionMilliseconds=semantic.semanticGenerationMs;if(timings.sourceAnalysisMilliseconds==0&&timings.semanticConstructionMilliseconds==0)timings.semanticConstructionMilliseconds=phase.elapsed();
        if(progress)progress({PrintPreparationPhase::SemanticConstruction,0,0});
        if(cancellation&&cancellation->isCancelled())return cancelled(sourceAnalysis,request.profile.identity,timings);
        if(semantic.status==LDrawSemanticOperandBuilder::Status::Cancelled)return cancelled(sourceAnalysis,request.profile.identity,timings);
        if(!sourceAnalysis.finite||!sourceAnalysis.indicesValid||sourceAnalysis.resourceLimitExceeded)return failure(sourceAnalysis.resourceLimitExceeded?PrintPreparationError::ResourceLimitExceeded:PrintPreparationError::InvalidSource,sourceAnalysis,request.profile.identity,"Source geometry is unsafe for semantic preparation.");
        if(!semantic.ok()){
            QString solidifierDiagnostic;
            if(!m_semanticBuilder&&supportsBoundedSourceSolidification(request.loadResult,sourceAnalysis)){phase.restart();auto solidified=SourceSurfaceSolidifier::solidify(request.loadResult);
                // An axial open attachment can be invisible to one winding-ray
                // direction. For a certified friction socket, try an orthogonal
                // ray against the same complete source before rejecting it.
                if(!solidified.successful&&BallSocketSemantic::recognize(request.loadResult).size()==1){
                    auto alternative=SourceSurfaceSolidifier::solidify(request.loadResult,.15,
                        SourceSurfaceSolidifier::RayAxis::Y);
                    if(alternative.successful)solidified=std::move(alternative);
                    else solidifierDiagnostic=solidified.diagnostic+QStringLiteral(" Orthogonal ray: ")+alternative.diagnostic;
                }
                timings.semanticConstructionMilliseconds+=phase.elapsed();if(solidified.successful){auto result=solidifiedResult(request,sourceAnalysis,std::move(solidified),timings,total.elapsed());if(result.ready())m_cache->insert(key,result.preparedMesh);return result;solidifierDiagnostic=result.diagnostic;}else if(solidifierDiagnostic.isEmpty())solidifierDiagnostic=solidified.diagnostic;}
            auto error=mapSemanticError(semantic.status);return failure(error,sourceAnalysis,request.profile.identity,semantic.diagnostics.join(' ')+(solidifierDiagnostic.isEmpty()?QString():QStringLiteral(" Bounded source-surface solidification: ")+solidifierDiagnostic));}
        if(std::size_t(semantic.operands.size())>request.profile.maximumOperands)return failure(PrintPreparationError::ResourceLimitExceeded,sourceAnalysis,request.profile.identity,"Semantic operand limit exceeded.");
        if(std::any_of(semantic.operands.cbegin(),semantic.operands.cend(),[](const auto&operand){return operand.confidence!=SemanticConfidence::HighConfidence;}))return failure(PrintPreparationError::AmbiguousSemantics,sourceAnalysis,request.profile.identity,"Automatic preparation requires HighConfidence semantic operands.");
        QVector<SemanticOperand> operands=semantic.operands;
        QString assemblyDiagnostic;
        if(!composeCertifiedBallAssembly(request.loadResult,&operands,&assemblyDiagnostic))
            return failure(PrintPreparationError::OperandClosureFailed,sourceAnalysis,request.profile.identity,assemblyDiagnostic);
        std::stable_sort(operands.begin(),operands.end(),[](const auto&a,const auto&b){const auto ar=roleOrder(a.role),br=roleOrder(b.role);if(ar!=br)return ar<br;if(a.compositionPriority!=b.compositionPriority)return a.compositionPriority<b.compositionPriority;return identity(a)<identity(b);});
        const auto primaryCount=std::count_if(operands.cbegin(),operands.cend(),[](const auto&o){return o.role==SemanticRole::PrimaryBody;});if(primaryCount!=1)return failure(PrintPreparationError::AmbiguousSemantics,sourceAnalysis,request.profile.identity,"Exactly one primary body/cavity operand is required.");
        if(progress)progress({PrintPreparationPhase::OperandValidation,0,int(operands.size())});
        phase.restart();for(int i=0;i<operands.size();++i){if(cancellation&&cancellation->isCancelled())return cancelled(sourceAnalysis,request.profile.identity,timings);if(exceedsMeshLimits(operands[i].closedMesh,request.profile))return failure(PrintPreparationError::ResourceLimitExceeded,sourceAnalysis,request.profile.identity,QString("Semantic operand %1 exceeds mesh resource limits.").arg(i));const auto valid=validateBooleanOperand(operands[i].analysis);if(!valid.ok())return failure(PrintPreparationError::OperandValidationFailed,sourceAnalysis,request.profile.identity,QString("Semantic operand %1 is invalid: %2").arg(i).arg(QString::fromStdString(valid.message)));}timings.operandValidationMilliseconds=phase.elapsed();
        PrintPreparationResult result;result.sourceAnalysis=sourceAnalysis;result.semanticOperandCount=std::size_t(operands.size());result.preparationProfileIdentity=request.profile.identity;PrintMesh accumulated=operands.front().closedMesh;phase.restart();
        if(progress)progress({PrintPreparationPhase::BooleanComposition,0,int(operands.size()-1)});
        for(int i=1;i<operands.size();++i){if(cancellation&&cancellation->isCancelled())return cancelled(sourceAnalysis,request.profile.identity,timings);if(exceedsMeshLimits(accumulated,request.profile))return failure(PrintPreparationError::ResourceLimitExceeded,sourceAnalysis,request.profile.identity,QString("Accumulated Boolean source before operation %1 exceeds mesh resource limits.").arg(i));const auto accumulatedAnalysis=analyzeSource(accumulated,{true,request.profile.maximumIntersectionCandidates});const auto accumulatedValid=validateBooleanOperand(accumulatedAnalysis);if(!accumulatedValid.ok())return failure(PrintPreparationError::BooleanResultInvalid,sourceAnalysis,request.profile.identity,QString("Accumulated Boolean source before operation %1 is invalid: %2").arg(i).arg(QString::fromStdString(accumulatedValid.message)));BooleanOperationSummary summary;summary.sequence=i;summary.role=operands[i].role;summary.feature=operands[i].feature;summary.sourceIdentity=identity(operands[i]);summary.sourceTriangles=accumulated.faces.size();summary.additiveTriangles=operands[i].closedMesh.faces.size();QElapsedTimer operation;operation.start();auto booleanResult=operands[i].role==SemanticRole::SubtractivePassage?backend->subtract(accumulated,operands[i].closedMesh):backend->unite(accumulated,operands[i].closedMesh);summary.elapsedMilliseconds=operation.elapsed();if(cancellation&&cancellation->isCancelled())return cancelled(sourceAnalysis,request.profile.identity,timings);if(!booleanResult.ok()){summary.successful=false;result.operations.push_back(summary);const auto error=booleanResult.error==MeshBooleanError::InvalidResult?PrintPreparationError::BooleanResultInvalid:PrintPreparationError::BooleanFailed;auto failed=failure(error,sourceAnalysis,request.profile.identity,QString("Boolean operation %1 failed for %2: %3").arg(i).arg(summary.sourceIdentity,QString::fromStdString(booleanResult.message)));failed.operations=result.operations;failed.finalAnalysis=booleanResult.resultAnalysis;return failed;}if(exceedsMeshLimits(booleanResult.mesh,request.profile)){auto failed=failure(PrintPreparationError::ResourceLimitExceeded,sourceAnalysis,request.profile.identity,QString("Boolean result %1 exceeds mesh resource limits.").arg(i));failed.finalAnalysis=booleanResult.resultAnalysis;return failed;}const auto valid=validateBooleanOperand(booleanResult.resultAnalysis);if(!valid.ok()){summary.resultTriangles=booleanResult.resultAnalysis.triangles;summary.resultComponents=booleanResult.resultAnalysis.connectedComponents;result.operations.push_back(summary);auto failed=failure(PrintPreparationError::BooleanResultInvalid,sourceAnalysis,request.profile.identity,QString("Boolean result %1 was rejected: %2").arg(i).arg(QString::fromStdString(valid.message)));failed.operations=result.operations;failed.finalAnalysis=booleanResult.resultAnalysis;return failed;}summary.successful=true;summary.resultTriangles=booleanResult.resultAnalysis.triangles;summary.resultComponents=booleanResult.resultAnalysis.connectedComponents;result.operations.push_back(summary);accumulated=std::move(booleanResult.mesh);}
        timings.booleanCompositionMilliseconds=phase.elapsed();if(cancellation&&cancellation->isCancelled())return cancelled(sourceAnalysis,request.profile.identity,timings);if(exceedsMeshLimits(accumulated,request.profile))return failure(PrintPreparationError::ResourceLimitExceeded,sourceAnalysis,request.profile.identity,"Prepared geometry exceeds mesh resource limits.");if(progress)progress({PrintPreparationPhase::FinalValidation,0,0});phase.restart();auto finalAnalysis=analyzeSource(accumulated,{true,request.profile.maximumIntersectionCandidates});timings.finalValidationMilliseconds=phase.elapsed();const auto finalValid=validatePreparedMesh(finalAnalysis);if(!finalValid.ok()){auto failed=failure(finalAnalysis.resourceLimitExceeded?PrintPreparationError::ResourceLimitExceeded:PrintPreparationError::FinalValidationFailed,sourceAnalysis,request.profile.identity,QString("Prepared geometry failed final validation: %1").arg(QString::fromStdString(finalValid.message)));failed.finalAnalysis=finalAnalysis;return failed;}
        result.dimensionalFidelity.sourceDimensions=dimensions(sourceAnalysis.bounds);result.dimensionalFidelity.preparedDimensions=dimensions(finalAnalysis.bounds);result.dimensionalFidelity.absoluteDimensionDifference={std::abs(result.dimensionalFidelity.sourceDimensions.x-result.dimensionalFidelity.preparedDimensions.x),std::abs(result.dimensionalFidelity.sourceDimensions.y-result.dimensionalFidelity.preparedDimensions.y),std::abs(result.dimensionalFidelity.sourceDimensions.z-result.dimensionalFidelity.preparedDimensions.z)};result.dimensionalFidelity.maximumBoundsDeviationMillimetres=maximumBoundsDeviation(sourceAnalysis.bounds,finalAnalysis.bounds);result.dimensionalFidelity.allowedBoundsDeviationMillimetres=request.profile.maximumExternalBoundsDeviationMillimetres;if(result.dimensionalFidelity.maximumBoundsDeviationMillimetres>request.profile.maximumExternalBoundsDeviationMillimetres){auto failed=failure(PrintPreparationError::DimensionalFidelityFailed,sourceAnalysis,request.profile.identity,"Prepared geometry exceeded the external-bounds preservation limit.");failed.finalAnalysis=finalAnalysis;failed.dimensionalFidelity=result.dimensionalFidelity;return failed;}
        auto prepared=std::make_shared<PreparedMesh>();
        prepared->mesh=std::move(accumulated);prepared->millimetreBounds=finalAnalysis.bounds;
        prepared->sourceAnalysis=sourceAnalysis;prepared->finalAnalysis=finalAnalysis;
        prepared->componentCount=finalAnalysis.connectedComponents;prepared->semanticOperandCount=std::size_t(operands.size());for(const auto&operand:operands)prepared->functionalFeatures.append(operand.functionalFeatures);
        // A certified joint8ball head can span multiple source-surface groups
        // before Boolean composition. Preserve its identity only after the
        // complete nominal mesh has passed strict final validation.
        for(const auto& ball:BallJointSemantic::recognize(request.loadResult)) {
            const bool retained=std::any_of(prepared->functionalFeatures.cbegin(),prepared->functionalFeatures.cend(),
                [&](const FunctionalFeature& feature){return feature.stableIdentity==ball.stableIdentity;});
            if(!retained)prepared->functionalFeatures.push_back(ball);
        }
        for(const auto& socket:BallSocketSemantic::recognize(request.loadResult)) {
            const bool retained=std::any_of(prepared->functionalFeatures.cbegin(),prepared->functionalFeatures.cend(),
                [&](const FunctionalFeature& feature){return feature.stableIdentity==socket.stableIdentity;});
            if(!retained)prepared->functionalFeatures.push_back(socket);
        }
        for(const auto& stud:certifiedSourceStuds(request.loadResult)) {
            const bool retained=std::any_of(prepared->functionalFeatures.cbegin(),prepared->functionalFeatures.cend(),
                [&](const FunctionalFeature& feature){return feature.stableIdentity==stud.feature.stableIdentity;});
            if(!retained)prepared->functionalFeatures.push_back(stud.feature);
        }
        prepared->partReference=request.partReference;prepared->ldrawIdentity=request.ldrawIdentity;
        prepared->dependencyFingerprint=request.loadResult.dependencyFingerprint;
        prepared->preparationProfileVersion=request.profile.identity;prepared->mcutVersion=backend->versionIdentity();
        prepared->preparationMethod="LDraw semantic operands with incremental MCUT Boolean composition";prepared->operations=result.operations;
        for(const auto&o:result.operations)prepared->operationSummary<<QString("%1 %2 %3: %4 and %5 -> %6 triangles").arg(o.role==SemanticRole::SubtractivePassage?"subtract":"union").arg(o.sequence).arg(o.sourceIdentity).arg(o.sourceTriangles).arg(o.additiveTriangles).arg(o.resultTriangles);
        prepared->sourceTriangleCount=sourceAnalysis.triangles;prepared->preparedTriangleCount=finalAnalysis.triangles;
        prepared->elapsedMilliseconds=total.elapsed();timings.totalMilliseconds=total.elapsed();prepared->timings=timings;prepared->dimensionalFidelity=result.dimensionalFidelity;prepared->warnings=result.warnings;
        result.state=PrintPreparationState::Ready;result.error=PrintPreparationError::None;result.preparedMesh=prepared;
        result.finalAnalysis=finalAnalysis;result.timings=timings;result.diagnostic="Nominal PreparedMesh independently validated.";
        m_cache->insert(key,prepared);return result;
    }catch(const std::exception&e){return failure(PrintPreparationError::BackendFailure,sourceAnalysis,request.profile.identity,QString("Print preparation backend failure: %1").arg(e.what()));}
}
}

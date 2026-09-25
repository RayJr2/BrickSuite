#include "../src/services/geometry/print/LDrawPrintPreparationService.h"
#include "../src/services/geometry/print/PlanarLocalComposer.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include "../src/services/geometry/print/LDrawPrintGeometryBuilder.h"
#include "../src/services/geometry/LDrawLibraryService.h"
#include <QCoreApplication>
#include <QTextStream>
#include <QTimeZone>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
using namespace PrintGeometry;
namespace {
bool check(bool v,const char*m){if(!v)QTextStream(stderr)<<"FAIL: "<<m<<Qt::endl;return v;}
bool check(bool v,const QString&m){if(!v)QTextStream(stderr)<<"FAIL: "<<m<<Qt::endl;return v;}
PrintMesh cube(double lo=0,double hi=1){PrintMesh m;m.vertices={{lo,lo,lo},{hi,lo,lo},{hi,hi,lo},{lo,hi,lo},{lo,lo,hi},{hi,lo,hi},{hi,hi,hi},{lo,hi,hi}};m.faces={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};return m;}
LDrawGeometry::LDrawLoadResult load(){LDrawGeometry::LDrawLoadResult r;r.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>();r.sourceModel->files.push_back({0,"parts/test.dat",LDrawGeometry::SourceClassification::Part});r.dependencyFingerprint.dependencies.push_back({"parts/test.dat",1,QDateTime::fromMSecsSinceEpoch(1,QTimeZone::UTC)});LDrawGeometry::Triangle t;t.a={0,0,0};t.b={1,0,0};t.c={0,1,0};r.mesh.triangles.push_back(t);return r;}
LDrawSemanticOperandBuilder::Result semantic(){LDrawSemanticOperandBuilder::Result r;r.status=LDrawSemanticOperandBuilder::Status::Ready;r.source=cube();r.sourceAnalysis=analyzeSource(r.source);r.coverage.route=SourceCoverageRoute::SemanticOperands;r.coverage.expandedTriangleCount=1;r.coverage.stitchedTriangleCount=1;r.coverage.groupedTriangleCount=1;r.coverage.expandedTriangleForStitchedTriangle={0};r.coverage.groupForStitchedTriangle={0};r.coverage.groups.push_back({0,1,SourceGroupCoverage::SemanticOperand});SemanticOperand primary;primary.role=SemanticRole::PrimaryBody;primary.feature=SemanticFeature::BodyOrCavity;primary.confidence=SemanticConfidence::HighConfidence;primary.closedMesh=cube();primary.analysis=analyzeSource(primary.closedMesh);primary.sourceFiles={"body.dat"};SemanticOperand stud;stud.role=SemanticRole::AdditiveAttachment;stud.feature=SemanticFeature::Stud;stud.confidence=SemanticConfidence::HighConfidence;stud.closedMesh=cube(.2,.8);stud.analysis=analyzeSource(stud.closedMesh);stud.sourceFiles={"stud.dat"};r.operands={stud,primary};return r;}
struct FakeState{int calls=0;CancellationState*cancel=nullptr;QString version="fake-1";enum Mode{Success,Fail,Invalid,Shifted}mode=Success;};
class FakeBoolean final:public MeshBooleanService{public:explicit FakeBoolean(std::shared_ptr<FakeState>s):state(std::move(s)){}MeshBooleanResult unite(const PrintMesh&source,const PrintMesh&additive)override{Q_UNUSED(additive);++state->calls;MeshBooleanResult r;r.sourceAnalysis=analyzeSource(source);r.additiveAnalysis=analyzeSource(additive);if(state->mode==FakeState::Fail){r.error=MeshBooleanError::BackendFailure;r.message="controlled failure";return r;}r.mesh=source;if(state->mode==FakeState::Invalid)r.mesh.faces.pop_back();if(state->mode==FakeState::Shifted)for(auto&p:r.mesh.vertices)p.x+=.01;r.resultAnalysis=analyzeSource(r.mesh);r.error=state->mode==FakeState::Invalid?MeshBooleanError::InvalidResult:MeshBooleanError::None;if(state->cancel)state->cancel->cancel();return r;}MeshBooleanResult subtract(const PrintMesh&source,const PrintMesh&passage)override{return unite(source,passage);}QString versionIdentity()const override{return state->version;}std::shared_ptr<FakeState>state;};
PrintPreparationRequest request(){PrintPreparationRequest r;r.partReference="test";r.ldrawIdentity="test";r.libraryAuthority="test-library";r.loadResult=load();return r;}
bool checkLocalPlateCorpus(const QString& library)
{
    bool ok=true;
    for(const auto& [part,expectedOperands]:std::array<std::pair<QString,int>,6>{{
            {QStringLiteral("3006"),30},{QStringLiteral("3031"),26},{QStringLiteral("3032"),40},
            {QStringLiteral("3035"),54},{QStringLiteral("3036"),84},{QStringLiteral("3033"),106}}}){
        PrintPreparationRequest plate;
        plate.partReference=part;plate.ldrawIdentity=QStringLiteral("parts/")+part+QStringLiteral(".dat");
        plate.libraryAuthority=library;plate.loadResult=LDrawLibraryService::loadPart(library,part);
        ok&=check(plate.loadResult.ok(),part+QStringLiteral(" authoritative plate source loads"));
        if(!plate.loadResult.ok())continue;
        const auto sourceTriangles=plate.loadResult.mesh.triangles.size();
        const auto prepared=LDrawPrintPreparationService().prepare(plate);
        ok&=check(prepared.ready()&&prepared.semanticOperandCount==std::size_t(expectedOperands)&&
            prepared.operations.isEmpty()&&prepared.sourceCoverage.complete()&&
            prepared.preparedMesh->preparationMethod.contains(QStringLiteral("local"),Qt::CaseInsensitive)&&
            prepared.finalAnalysis.connectedComponents==1&&prepared.finalAnalysis.boundaryEdges==0&&
            prepared.dimensionalFidelity.maximumBoundsDeviationMillimetres<1e-4&&
            plate.loadResult.mesh.triangles.size()==sourceTriangles,
            part+QStringLiteral(" locally composes complete nominal source without sequential Booleans: ")+prepared.diagnostic);
        if(part!=QStringLiteral("3032")||!prepared.ready())continue;
        const auto sourceSemantics=LDrawSemanticOperandBuilder::build(plate.loadResult);
        const auto studGroups=std::count_if(sourceSemantics.operands.cbegin(),sourceSemantics.operands.cend(),
            [](const SemanticOperand& operand){return operand.feature==SemanticFeature::Stud;});
        const auto tubeGroups=std::count_if(sourceSemantics.operands.cbegin(),sourceSemantics.operands.cend(),
            [](const SemanticOperand& operand){return operand.feature==SemanticFeature::Tube;});
        const auto retainedStuds=std::count_if(prepared.preparedMesh->functionalFeatures.cbegin(),
            prepared.preparedMesh->functionalFeatures.cend(),[](const FunctionalFeature& feature){
                return feature.family==FunctionalInterfaceFamily::StandardStud;
            });
        ok&=check(studGroups==24&&tubeGroups==15&&retainedStuds>=24&&
            prepared.diagnostic.contains(QStringLiteral("39 disjoint local surface regions")),
            QStringLiteral("3032 retains all 24 stud and 15 tube contact regions with stud fit ownership"));
        const auto again=LDrawPrintPreparationService().prepare(plate);
        bool sameVertices=again.ready()&&again.preparedMesh->mesh.vertices.size()==prepared.preparedMesh->mesh.vertices.size();
        if(sameVertices)for(std::size_t i=0;i<again.preparedMesh->mesh.vertices.size();++i){
            const auto&a=again.preparedMesh->mesh.vertices[i],&b=prepared.preparedMesh->mesh.vertices[i];
            if(a.x!=b.x||a.y!=b.y||a.z!=b.z){sameVertices=false;break;}
        }
        ok&=check(sameVertices&&again.preparedMesh->mesh.faces==prepared.preparedMesh->mesh.faces,
            QStringLiteral("3032 local composition is deterministic across independent service calls"));
        auto overlapping=sourceSemantics.operands;overlapping.push_back(overlapping.back());
        ok&=check(!PlanarLocalComposer::compose(overlapping).successful,
            QStringLiteral("overlapping source attachment windows cannot take local composition route"));
        auto wrongSide=sourceSemantics.operands;
        for(auto& vertex:wrongSide.back().sourceMesh.vertices)vertex.z=-vertex.z;
        ok&=check(!PlanarLocalComposer::compose(wrongSide).successful,
            QStringLiteral("attachment crossing its body plane cannot take local composition route"));
    }
    return ok;
}
}
int main(int argc,char**argv){QCoreApplication app(argc,argv);bool ok=true;const auto diagnosticArguments=app.arguments();const int sourceOnlyAt=diagnosticArguments.indexOf(QStringLiteral("--source-only"));const int semanticOnlyAt=diagnosticArguments.indexOf(QStringLiteral("--semantic-only"));const int prepareOnlyAt=diagnosticArguments.indexOf(QStringLiteral("--prepare-only"));const int auditAt=sourceOnlyAt>=0?sourceOnlyAt:semanticOnlyAt>=0?semanticOnlyAt:prepareOnlyAt;if(auditAt>=0&&auditAt+2<diagnosticArguments.size()){const auto loaded=LDrawLibraryService::loadPart(diagnosticArguments[auditAt+1],diagnosticArguments[auditAt+2]);QTextStream(stdout)<<"source-load-ok="<<loaded.ok()<<" triangles="<<loaded.mesh.triangles.size()<<" references="<<(loaded.sourceModel?loaded.sourceModel->references.size():0)<<" surfaces="<<(loaded.sourceModel?loaded.sourceModel->surfaces.size():0)<<Qt::endl;if(!loaded.ok())return 1;if(semanticOnlyAt>=0){const auto semantic=LDrawSemanticOperandBuilder::build(loaded);QTextStream(stdout)<<"semantic-status="<<int(semantic.status)<<" groups="<<semantic.semanticGroups<<" operands="<<semantic.operands.size()<<" source-components="<<semantic.sourceAnalysis.connectedComponents<<" source-boundaries="<<semantic.sourceAnalysis.boundaryEdges<<" source-ms="<<semantic.sourceConversionAnalysisMs<<" semantic-ms="<<semantic.semanticGenerationMs<<" coverage-bytes="<<(semantic.coverage.expandedTriangleForStitchedTriangle.size()+semantic.coverage.groupForStitchedTriangle.size())*sizeof(int)<<" coverage-complete="<<semantic.coverage.complete()<<Qt::endl;}if(prepareOnlyAt>=0){PrintPreparationRequest audit;audit.partReference=diagnosticArguments[auditAt+2];audit.ldrawIdentity=QStringLiteral("parts/")+audit.partReference+QStringLiteral(".dat");audit.libraryAuthority=diagnosticArguments[auditAt+1];audit.loadResult=loaded;const auto prepared=LDrawPrintPreparationService().prepare(audit);QTextStream(stdout)<<"ready="<<prepared.ready()<<" error="<<int(prepared.error)<<" operands="<<prepared.semanticOperandCount<<" boolean-operations="<<prepared.operations.size()<<" source-ms="<<prepared.timings.sourceAnalysisMilliseconds<<" semantic-ms="<<prepared.timings.semanticConstructionMilliseconds<<" total-ms="<<prepared.timings.totalMilliseconds<<" diagnostic="<<prepared.diagnostic<<Qt::endl;}return 0;}auto state=std::make_shared<FakeState>();auto cache=std::make_shared<PrintPreparationCache>(2,1024*1024);auto factory=[state]{return std::make_unique<FakeBoolean>(state);};auto builder=[](const auto&){return semantic();};LDrawPrintPreparationService service(cache,factory,builder);
 const int localAt=diagnosticArguments.indexOf(QStringLiteral("--local-ldraw"));
 if(localAt>=0)return localAt+1<diagnosticArguments.size()&&checkLocalPlateCorpus(diagnosticArguments[localAt+1])?0:1;
 const auto arguments=app.arguments();const int libraryAt=arguments.indexOf(QStringLiteral("--ldraw"));if(libraryAt>=0&&libraryAt+1<arguments.size()){const auto loaded=LDrawLibraryService::loadPart(arguments[libraryAt+1],QStringLiteral("2780"));ok&=check(loaded.ok(),"real 2780 loads for end-to-end preparation");if(loaded.ok()){PrintPreparationRequest realRequest;realRequest.partReference=QStringLiteral("2780");realRequest.ldrawIdentity=QStringLiteral("parts/2780.dat");realRequest.libraryAuthority=arguments[libraryAt+1];realRequest.loadResult=loaded;LDrawPrintPreparationService realService;const auto realPrepared=realService.prepare(realRequest);ok&=check(realPrepared.ready(),QStringLiteral("real 2780 reaches Ready PreparedMesh through the production service: ")+realPrepared.diagnostic);if(realPrepared.ready()){ok&=check(realPrepared.finalAnalysis.boundaryEdges==0&&realPrepared.finalAnalysis.nonManifoldEdges==0&&realPrepared.finalAnalysis.selfIntersections==0&&realPrepared.finalAnalysis.connectedComponents==1,"real 2780 PreparedMesh independently validates as one manifold solid");ok&=check(realPrepared.preparedMesh->preparationMethod.contains(QStringLiteral("source-surface"),Qt::CaseInsensitive),"real 2780 uses reusable source-surface preparation");}}}
 if(libraryAt>=0&&libraryAt+1<arguments.size())for(const QString& part:{QStringLiteral("4275a"),QStringLiteral("4276a")}) {
    PrintPreparationRequest hingeRequest;
    hingeRequest.partReference=part;
    hingeRequest.ldrawIdentity=QStringLiteral("parts/")+part+QStringLiteral(".dat");
    hingeRequest.libraryAuthority=arguments[libraryAt+1];
    hingeRequest.loadResult=LDrawLibraryService::loadPart(arguments[libraryAt+1],part);
    LDrawPrintPreparationService hingeService;
    const auto prepared=hingeService.prepare(hingeRequest);
    ok&=check(prepared.ready(),part+QStringLiteral(" reaches production PreparedMesh: ")+prepared.diagnostic);
    if(prepared.ready()) {
        ok&=check(prepared.finalAnalysis.connectedComponents==1&&prepared.finalAnalysis.boundaryEdges==0&&
            prepared.finalAnalysis.nonManifoldEdges==0,part+QStringLiteral(" prepared manifold"));
        ok&=check(std::any_of(prepared.preparedMesh->functionalFeatures.cbegin(),
            prepared.preparedMesh->functionalFeatures.cend(),[](const FunctionalFeature& feature) {
                return feature.family==FunctionalInterfaceFamily::InterleavedFingerHinge;
            }),
            part+QStringLiteral(" retains certified finger-hinge identity"));
    }
 }
 if(libraryAt>=0&&libraryAt+1<arguments.size())for(const QString& part:{QStringLiteral("30345"),QStringLiteral("76385")}) {
    PrintPreparationRequest hingeRequest;
    hingeRequest.partReference=part;
    hingeRequest.ldrawIdentity=QStringLiteral("parts/")+part+QStringLiteral(".dat");
    hingeRequest.libraryAuthority=arguments[libraryAt+1];
    hingeRequest.loadResult=LDrawLibraryService::loadPart(arguments[libraryAt+1],part);
    LDrawPrintPreparationService hingeService;
    const auto prepared=hingeService.prepare(hingeRequest);
    ok&=check(prepared.ready(),part+QStringLiteral(" reaches production PreparedMesh: ")+prepared.diagnostic);
    if(prepared.ready()) {
        ok&=check(validatePreparedMesh(prepared.finalAnalysis).ok(),part+QStringLiteral(" click hinge prepared manifold"));
        ok&=check(std::any_of(prepared.preparedMesh->functionalFeatures.cbegin(),
            prepared.preparedMesh->functionalFeatures.cend(),[](const FunctionalFeature& feature) {
                return feature.family==FunctionalInterfaceFamily::ClickHinge;
            }),part+QStringLiteral(" retains certified click hinge identity after preparation"));
    }
 }
 if(libraryAt>=0&&libraryAt+1<arguments.size())for(const QString& part:{QStringLiteral("30027b"),QStringLiteral("4488")}) {
    PrintPreparationRequest wheelRequest;
    wheelRequest.partReference=part;
    wheelRequest.ldrawIdentity=QStringLiteral("parts/")+part+QStringLiteral(".dat");
    wheelRequest.libraryAuthority=arguments[libraryAt+1];
    wheelRequest.loadResult=LDrawLibraryService::loadPart(arguments[libraryAt+1],part);
    LDrawPrintPreparationService wheelService;
    const auto prepared=wheelService.prepare(wheelRequest);
    ok&=check(prepared.ready(),part+QStringLiteral(" retained-wheel Part prepares: ")+prepared.diagnostic);
    if(prepared.ready())ok&=check(validatePreparedMesh(prepared.finalAnalysis).ok()&&
        std::any_of(prepared.preparedMesh->functionalFeatures.cbegin(),
            prepared.preparedMesh->functionalFeatures.cend(),[&](const FunctionalFeature& feature){
                return feature.family==FunctionalInterfaceFamily::RetainedRotatingWheel&&
                    feature.role==(part==QStringLiteral("4488")?FunctionalInterfaceRole::Male:
                        FunctionalInterfaceRole::Female);
            }),part+QStringLiteral(" retains certified wheel interface after preparation"));
    if(part==QStringLiteral("4488")&&prepared.ready())
        ok&=check(prepared.preparedMesh->preparationMethod.contains(QStringLiteral("source-surface"),Qt::CaseInsensitive)&&
                  prepared.diagnostic.contains(QStringLiteral("ray axis 1")),
                  "real 4488 uses validated orthogonal source-surface solidification");
    if(part==QStringLiteral("4488")){
        auto uncertified=wheelRequest;
        uncertified.loadResult.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>(
            *wheelRequest.loadResult.sourceModel);
        uncertified.loadResult.sourceModel->surfaces.front().certified=false;
        ok&=check(!LDrawPrintPreparationService().prepare(uncertified).ready(),
                  "orthogonal wheel-pin preparation rejects uncertified source topology");
    }
 }
 if(libraryAt>=0&&libraryAt+1<arguments.size()){
    PrintPreparationRequest plainRequest;
    plainRequest.partReference=QStringLiteral("30027a");
    plainRequest.ldrawIdentity=QStringLiteral("parts/30027a.dat");
    plainRequest.libraryAuthority=arguments[libraryAt+1];
    plainRequest.loadResult=LDrawLibraryService::loadPart(arguments[libraryAt+1],QStringLiteral("30027a"));
    LDrawPrintPreparationService plainService;
    const auto prepared=plainService.prepare(plainRequest);
    ok&=check(prepared.ready(),QStringLiteral("real 30027a plain wheel prepares: ")+prepared.diagnostic);
    if(prepared.ready())ok&=check(validatePreparedMesh(prepared.finalAnalysis).ok()&&
        std::any_of(prepared.preparedMesh->functionalFeatures.cbegin(),
            prepared.preparedMesh->functionalFeatures.cend(),[](const FunctionalFeature& feature){
                return feature.family==FunctionalInterfaceFamily::PlainRoundBoreWheel&&
                    feature.role==FunctionalInterfaceRole::Female;
            }),QStringLiteral("real 30027a retains certified blind-bore identity after preparation"));
 }
 if(libraryAt>=0&&libraryAt+1<arguments.size()){
    const QString library=arguments[libraryAt+1];
    for(const QString& part:{QStringLiteral("3001"),QStringLiteral("3700"),QStringLiteral("22890"),QStringLiteral("76385"),QStringLiteral("4488"),QStringLiteral("3032"),
                             QStringLiteral("3037"),QStringLiteral("32064a"),QStringLiteral("6553"),QStringLiteral("10113"),QStringLiteral("11399")}){
        PrintPreparationRequest corpus;
        corpus.partReference=part;corpus.ldrawIdentity=QStringLiteral("parts/")+part+QStringLiteral(".dat");
        corpus.libraryAuthority=library;corpus.loadResult=LDrawLibraryService::loadPart(library,part);
        ok&=check(corpus.loadResult.ok(),part+QStringLiteral(" authoritative source loads"));
        if(!corpus.loadResult.ok())continue;
        const auto prepared=LDrawPrintPreparationService().prepare(corpus);
        if(arguments.contains(QStringLiteral("--coverage-report"))){
            QStringList uncovered;for(int group:prepared.sourceCoverage.uncoveredGroups())uncovered<<QString::number(group);
            QTextStream(stdout)<<part<<" ready="<<prepared.ready()
                <<" expanded="<<prepared.sourceCoverage.expandedTriangleCount
                <<" stitched="<<prepared.sourceCoverage.stitchedTriangleCount
                <<" groups="<<prepared.sourceCoverage.groups.size()
                <<" represented="<<prepared.sourceCoverage.representedGroups()
                <<" excluded="<<prepared.sourceCoverage.excludedGroups()
                <<" uncovered="<<uncovered.join(',')
                <<" route="<<int(prepared.sourceCoverage.route)<<Qt::endl;
        }
        const bool passing=part==QStringLiteral("3001")||part==QStringLiteral("3700")||
            part==QStringLiteral("22890")||part==QStringLiteral("76385")||part==QStringLiteral("4488")||
            part==QStringLiteral("3032");
        ok&=check(prepared.ready()==passing,part+QStringLiteral(" retains corpus Ready/Not Ready behavior: ")+prepared.diagnostic);
        ok&=check(prepared.sourceCoverage.expandedTriangleCount==corpus.loadResult.mesh.triangles.size()&&
                  !prepared.sourceCoverage.groups.isEmpty(),part+QStringLiteral(" has authoritative stitched-source group accounting"));
        if(passing)ok&=check(prepared.sourceCoverage.complete()&&
                             prepared.sourceCoverage.representedGroups()==prepared.sourceCoverage.groups.size()&&
                             prepared.preparedMesh->sourceCoverage.complete(),
                             part+QStringLiteral(" Ready result covers every authoritative source group"));
        if(!passing)ok&=check(!prepared.sourceCoverage.uncoveredGroups().isEmpty(),
                       part+QStringLiteral(" Not Ready result identifies uncovered groups"));
    }
    ok&=checkLocalPlateCorpus(library);
 }
 auto uncoveredBuilder=[](const auto&){auto result=semantic();result.coverage.groups.push_back({1,1});result.coverage.expandedTriangleCount=2;result.coverage.stitchedTriangleCount=2;result.coverage.groupedTriangleCount=2;result.coverage.expandedTriangleForStitchedTriangle.push_back(1);result.coverage.groupForStitchedTriangle.push_back(1);return result;};
 LDrawPrintPreparationService uncoveredService({},factory,uncoveredBuilder);
 const auto uncovered=uncoveredService.prepare(request());
 ok&=check(!uncovered.ready()&&uncovered.error==PrintPreparationError::OperandValidationFailed&&
           uncovered.sourceCoverage.uncoveredGroups()==QVector<int>{1}&&state->calls==0,
           "unrepresented authoritative group is rejected before Boolean composition");
 auto crowdedBuilder=[](const auto&){auto result=semantic();for(int i=0;i<24;++i)result.operands.push_back(result.operands.front());return result;};
 LDrawPrintPreparationService crowdedService({},factory,crowdedBuilder);
 auto crowdedRequest=request();crowdedRequest.profile.maximumBooleanOperations=24;
 const auto crowded=crowdedService.prepare(crowdedRequest);
 ok&=check(crowded.error==PrintPreparationError::ResourceLimitExceeded&&crowded.operations.isEmpty()&&
           crowded.sourceCoverage.complete()&&state->calls==0,
           "Boolean-operation preflight rejects crowded composition before backend work");
 auto exclusion=semantic().coverage;exclusion.groups[0].status=SourceGroupCoverage::IntentionallyExcluded;
 ok&=check(!exclusion.complete(),"intentional source exclusion requires a stated rule");
 exclusion.groups[0].exclusionReason=QStringLiteral("synthetic test-only exclusion");
 ok&=check(exclusion.complete()&&exclusion.excludedGroups()==1,"explicit exclusion remains separately auditable");
 QVector<PrintPreparationProgress> progress;auto progressResult=service.prepare(request(),nullptr,[&progress](const auto&p){progress.push_back(p);});ok&=check(progressResult.ready()&&progress.size()>=5,"typed progress delivered");ok&=check(progress.front().phase==PrintPreparationPhase::SourceAnalysis&&progress.back().phase==PrintPreparationPhase::FinalValidation,"progress phase ordering");cache->clear();state->calls=0;
 const auto original=request();const auto originalPoint=original.loadResult.mesh.triangles.front().a;auto first=service.prepare(original);ok&=check(first.ready()&&!first.cacheHit,"ready synthetic preparation");ok&=check(first.sourceCoverage.complete()&&first.preparedMesh->sourceCoverage.complete(),"complete coverage retained on Ready result");ok&=check(state->calls==1&&first.operations.size()==1,"deterministic incremental union");ok&=check(first.preparedMesh->partReference=="test"&&first.preparedMesh->preparationProfileVersion==LDrawPrintPreparationProfile::Version,"prepared provenance");ok&=check(original.loadResult.mesh.triangles.front().a==originalPoint,"authoritative source remains immutable");auto second=service.prepare(original);ok&=check(second.ready()&&second.cacheHit&&state->calls==1,"cache hit avoids Boolean work");ok&=check(second.sourceCoverage.complete(),"cache hit retains source coverage");ok&=check(second.operations.size()==first.operations.size()&&second.dimensionalFidelity.maximumBoundsDeviationMillimetres==first.dimensionalFidelity.maximumBoundsDeviationMillimetres,"cache hit preserves Ready metadata");
 auto changed=request();changed.loadResult.dependencyFingerprint.dependencies[0].size=2;auto stale=service.prepare(changed);ok&=check(stale.ready()&&!stale.cacheHit&&state->calls==2,"fingerprint invalidates cache");changed=request();changed.partReference="other-part";auto otherPart=service.prepare(changed);ok&=check(!otherPart.cacheHit&&otherPart.preparedMesh->partReference=="other-part","Part identity invalidates cache provenance");changed=request();changed.ldrawIdentity="other";ok&=check(!service.prepare(changed).cacheHit,"candidate invalidates cache");changed=request();changed.profile.identity="other-profile";ok&=check(!service.prepare(changed).cacheHit,"profile invalidates cache");changed=request();changed.libraryAuthority="other-library";ok&=check(!service.prepare(changed).cacheHit,"authority invalidates cache");auto versionState=std::make_shared<FakeState>();versionState->version="fake-2";LDrawPrintPreparationService versionService(cache,[versionState]{return std::make_unique<FakeBoolean>(versionState);},builder);ok&=check(!versionService.prepare(request()).cacheHit&&versionState->calls==1,"MCUT version invalidates cache");
 auto ordered=request();ordered.loadResult.dependencyFingerprint.dependencies.push_back({"parts/second.dat",2,QDateTime::fromMSecsSinceEpoch(2,QTimeZone::UTC)});auto reversed=ordered;std::reverse(reversed.loadResult.dependencyFingerprint.dependencies.begin(),reversed.loadResult.dependencyFingerprint.dependencies.end());ok&=check(PrintPreparationCache::keyFor(ordered,"fake-1")==PrintPreparationCache::keyFor(reversed,"fake-1"),"dependency fingerprint order is stable");
 CancellationState already;already.cancel();ok&=check(service.prepare(request(),&already).state==PrintPreparationState::Cancelled,"cancel before work");
 auto cancelState=std::make_shared<FakeState>();CancellationState during;cancelState->cancel=&during;LDrawPrintPreparationService cancelService({},[cancelState]{return std::make_unique<FakeBoolean>(cancelState);},builder);ok&=check(cancelService.prepare(request(),&during).state==PrintPreparationState::Cancelled&&cancelState->calls==1,"cancel after active Boolean");
 auto failState=std::make_shared<FakeState>();failState->mode=FakeState::Fail;LDrawPrintPreparationService failService({},[failState]{return std::make_unique<FakeBoolean>(failState);},builder);auto failed=failService.prepare(request());ok&=check(failed.error==PrintPreparationError::BooleanFailed&&!failed.preparedMesh,"typed Boolean failure");
 auto invalidState=std::make_shared<FakeState>();invalidState->mode=FakeState::Invalid;LDrawPrintPreparationService invalidService({},[invalidState]{return std::make_unique<FakeBoolean>(invalidState);},builder);ok&=check(invalidService.prepare(request()).error==PrintPreparationError::BooleanResultInvalid,"invalid Boolean result distinguished");
 auto shiftedState=std::make_shared<FakeState>();shiftedState->mode=FakeState::Shifted;LDrawPrintPreparationService shiftedService({},[shiftedState]{return std::make_unique<FakeBoolean>(shiftedState);},builder);ok&=check(shiftedService.prepare(request()).error==PrintPreparationError::DimensionalFidelityFailed,"dimensional fidelity gate");
 auto unsupported=[](const auto&){auto r=semantic();r.status=LDrawSemanticOperandBuilder::Status::UnsupportedBoundaryTopology;r.diagnostics={"unsupported fixture"};return r;};LDrawPrintPreparationService unsupportedService({},factory,unsupported);ok&=check(unsupportedService.prepare(request()).state==PrintPreparationState::Unsupported,"typed unsupported semantics");auto ambiguous=[](const auto&){auto r=semantic();r.status=LDrawSemanticOperandBuilder::Status::AmbiguousBoundary;r.diagnostics={"ambiguous fixture"};return r;};LDrawPrintPreparationService ambiguousService({},factory,ambiguous);ok&=check(ambiguousService.prepare(request()).state==PrintPreparationState::Ambiguous,"typed ambiguous semantics");auto lowConfidence=[](const auto&){auto r=semantic();r.operands.back().confidence=SemanticConfidence::Ambiguous;return r;};LDrawPrintPreparationService lowConfidenceService({},factory,lowConfidence);ok&=check(lowConfidenceService.prepare(request()).error==PrintPreparationError::AmbiguousSemantics,"only HighConfidence semantics are automatic");
 auto limited=request();limited.profile.maximumFaces=0;ok&=check(service.prepare(limited).error==PrintPreparationError::ResourceLimitExceeded,"typed source resource limit");auto bad=request();bad.loadResult.mesh.triangles[0].a.setX(std::numeric_limits<float>::quiet_NaN());int callsBefore=state->calls;ok&=check(service.prepare(bad).error==PrintPreparationError::InvalidSource&&state->calls==callsBefore,"invalid source rejected before semantics/Boolean");
 PrintPreparationCache lru(2,1024*1024);auto mesh=std::make_shared<PreparedMesh>();auto a=PrintPreparationCache::keyFor(request(),"fake-1");auto b=a;b.ldrawIdentity="b";auto c=a;c.ldrawIdentity="c";lru.insert(a,mesh);lru.insert(b,mesh);ok&=check(bool(lru.find(a)),"LRU access");lru.insert(c,mesh);ok&=check(!lru.find(b)&&bool(lru.find(a))&&bool(lru.find(c)),"deterministic LRU eviction");ok&=check(lru.statistics().evictions==1,"eviction statistic");lru.clear();ok&=check(lru.statistics().entries==0,"cache clear seam");
 return ok?0:1;}

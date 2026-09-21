#include "../src/services/geometry/print/LDrawPrintPreparationService.h"
#include "../src/services/geometry/print/PreparedObjProofWriter.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/ui/parts/PreparedMeshRenderAdapter.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QTextStream>

#include <cmath>

using namespace PrintGeometry;

namespace {
bool check(bool value,const QString&message)
{
    if(!value)QTextStream(stderr)<<"FAIL: "<<message<<Qt::endl;
    return value;
}

const FunctionalFeature* roundFeature(const LDrawSemanticOperandBuilder::Result&result)
{
    for(const auto&operand:result.operands)
        if(operand.feature==SemanticFeature::RoundThroughPassage&&!operand.functionalFeatures.isEmpty())
            return &operand.functionalFeatures.front();
    return nullptr;
}
double featureDot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double featureNorm(Point p){return std::sqrt(featureDot(p,p));}
Point featureCross(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}

QVector3D sourcePoint(Point p){return QVector3D(float(p.x/0.4),float(-p.z/0.4),float(p.y/0.4));}
LDrawGeometry::LDrawLoadResult roundPassageFixture(bool certified=true,bool oneEnded=false,bool rotated=false,bool mirrored=false,bool connectionEvidence=true)
{
    LDrawGeometry::LDrawLoadResult result;result.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>();
    result.sourceModel->files={{0,"parts/synthetic.dat",LDrawGeometry::SourceClassification::Part},{1,connectionEvidence?"p/peghole.dat":"p/synthetic-round.dat",LDrawGeometry::SourceClassification::Primitive}};
    auto transform=[=](Point p){if(rotated)p={p.z,p.y,-p.x};if(mirrored)p.x=-p.x;return p;};
    const auto opening0=sourcePoint(transform({0,0,-2})),opening1=sourcePoint(transform({0,0,2}));
    std::array<double,12>reference0{1,0,0,double(opening0.x()),0,1,0,double(opening0.y()),0,0,1,double(opening0.z())},reference1{1,0,0,double(opening1.x()),0,1,0,double(opening1.y()),0,0,1,double(opening1.z())};
    result.sourceModel->references.push_back({0,-1,1,1,reference0,mirrored,false});
    result.sourceModel->references.push_back({1,-1,1,2,reference1,mirrored,false});
    auto triangle=[&](Point a,Point b,Point c,int file){if(mirrored)std::swap(b,c);LDrawGeometry::Triangle t;t.a=sourcePoint(transform(a));t.b=sourcePoint(transform(b));t.c=sourcePoint(transform(c));const int index=result.mesh.triangles.size();result.mesh.triangles<<t;const int reference=file==0?(index%2):0;result.sourceModel->surfaces<<LDrawGeometry::SurfaceRecord{index,reference,file,index+1,3,certified||file==0,true,false};};
    constexpr int OuterSegments=8,PassageSegments=16;const double pi=3.14159265358979323846;
    auto point=[](double radius,double angle,double z){return Point{radius*std::cos(angle),radius*std::sin(angle),z};};
    for(int i=0;i<OuterSegments;++i){const int n=(i+1)%OuterSegments;const double a=2*pi*i/OuterSegments,b=2*pi*n/OuterSegments;const auto bo=point(2,a,-2),bn=point(2,b,-2),to=point(2,a,2),tn=point(2,b,2),bi=point(1,a,-2),bni=point(1,b,-2),ti=point(1,a,2),tni=point(1,b,2);triangle(bo,bn,to,0);triangle(bn,tn,to,0);triangle(to,tn,ti,0);triangle(tn,tni,ti,0);triangle(bo,bi,bn,0);triangle(bn,bi,bni,0);}
    for(int i=0;i<PassageSegments;++i){const int n=(i+1)%PassageSegments;const double a=2*pi*i/PassageSegments,b=2*pi*n/PassageSegments;const auto low=point(1,a,-2),lowNext=point(1,b,-2),high=point(1,a,2),highNext=point(1,b,2);triangle(low,high,lowNext,1);triangle(lowNext,high,highNext,1);}
    if(oneEnded){const Point center{0,0,-2};for(int i=0;i<PassageSegments;++i){const int n=(i+1)%PassageSegments;triangle(center,point(1,2*pi*i/PassageSegments,-2),point(1,2*pi*n/PassageSegments,-2),1);}}
    return result;
}

bool segmentRemainsOpen(const PrintMesh&mesh,const Point&start,const Point&end)
{
    const Point direction{end.x-start.x,end.y-start.y,end.z-start.z};auto subtract=[](const Point&a,const Point&b){return Point{a.x-b.x,a.y-b.y,a.z-b.z};};auto cross=[](const Point&a,const Point&b){return Point{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};auto dot=[](const Point&a,const Point&b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    for(const auto&face:mesh.faces){const auto&a=mesh.vertices[face[0]],&b=mesh.vertices[face[1]],&c=mesh.vertices[face[2]];const Point e1=subtract(b,a),e2=subtract(c,a),p=cross(direction,e2);const double determinant=dot(e1,p);if(std::abs(determinant)<1e-10)continue;const double inverse=1.0/determinant;const Point t=subtract(start,a);const double u=dot(t,p)*inverse;if(u<0.0||u>1.0)continue;const Point q=cross(t,e1);const double v=dot(direction,q)*inverse;if(v<0.0||u+v>1.0)continue;const double distance=dot(e2,q)*inverse;if(distance>=0.0&&distance<=1.0)return false;}
    return true;
}
}

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);
    bool ok=true;
    PrintMesh component;
    component.vertices={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0},{-1,-1,2},{1,-1,2},{1,1,2},{-1,1,2}};
    const std::vector<std::uint32_t>loop{0,1,2,3};
    auto direction=boundaryAttachmentDirection(component,loop);
    ok&=check(direction.z<-.999,"topology-derived attachment direction");
    for(auto&point:component.vertices){const double x=point.x;point.x=point.z;point.z=-x;}
    direction=boundaryAttachmentDirection(component,loop);
    ok&=check(direction.x<-.999,"rotated attachment direction");

    const auto validFixture=roundPassageFixture();const auto semantic=LDrawSemanticOperandBuilder::build(validFixture);
    ok&=check(semantic.ok(),"certified round through-passage recognized");
    ok&=check(std::count_if(semantic.operands.cbegin(),semantic.operands.cend(),[](const auto&o){return o.role==SemanticRole::SubtractivePassage&&o.feature==SemanticFeature::RoundThroughPassage;})==1,"round passage semantic identity retained");
    const auto*feature=roundFeature(semantic);ok&=check(feature,"functional feature contract retained");
    if(feature){ok&=check(feature->role==FunctionalInterfaceRole::Female&&feature->materialSide==FunctionalMaterialSide::EmptyInsideMaterialOutside,"female clearance material side retained");ok&=check(feature->eligibility==FunctionalEligibility::Eligible&&feature->confidence==SemanticConfidence::HighConfidence,"feature is deterministically eligible");ok&=check(std::abs(feature->nominalRadiusMillimetres-1.0)<1e-9&&std::abs(feature->nominalDiameterMillimetres-2.0)<1e-9&&std::abs(feature->nominalAxialExtentMillimetres-4.0)<1e-9&&std::abs(feature->nominalEngagementExtentMillimetres-4.0)<1e-9&&!feature->radialProfile.isEmpty(),"authoritative nominal profile retained");ok&=check(std::abs(featureNorm(feature->frame.axis)-1.0)<1e-9&&std::abs(featureDot(feature->frame.axis,feature->frame.profileU))<1e-9&&std::abs(featureDot(feature->frame.axis,feature->frame.profileV))<1e-9&&featureDot(featureCross(feature->frame.axis,feature->frame.profileU),feature->frame.profileV)>.999,"stable right-handed local frame retained");ok&=check(!feature->stableIdentity.isEmpty()&&feature->governingOperandIdentity.startsWith(feature->stableIdentity)&&feature->constructionRecipe=="round-through-passage-v1","stable identity and governing recipe retained");ok&=check(feature->evidenceContract=="official-ldraw-peghole-pair-v1"&&!feature->provenance.isEmpty()&&feature->operandAction==FunctionalOperandAction::Subtract,"reviewed authoritative evidence and subtractive action retained");}
    const auto repeated=LDrawSemanticOperandBuilder::build(validFixture);const auto*repeatedFeature=roundFeature(repeated);ok&=check(repeated.ok()&&repeated.closureTriangles==semantic.closureTriangles&&repeated.operands.size()==semantic.operands.size()&&feature&&repeatedFeature&&feature->stableIdentity==repeatedFeature->stableIdentity,"round passage classification and identity deterministic");
    const auto rotated=LDrawSemanticOperandBuilder::build(roundPassageFixture(true,false,true));const auto*rotatedFeature=roundFeature(rotated);ok&=check(rotated.ok()&&rotatedFeature&&std::abs(std::abs(rotatedFeature->frame.axis.x)-1.0)<1e-9,"rotated round passage frame follows geometry");
    const auto mirrored=LDrawSemanticOperandBuilder::build(roundPassageFixture(true,false,false,true));const auto*mirroredFeature=roundFeature(mirrored);ok&=check(mirrored.ok()&&mirroredFeature&&mirroredFeature->frame.mirrored&&std::abs(featureNorm(mirroredFeature->frame.axis)-1.0)<1e-9,"mirrored round passage retains transform state and valid local frame");
    ok&=check(!LDrawSemanticOperandBuilder::build(roundPassageFixture(true,true)).ok(),"one-ended cavity rejected");
    ok&=check(!LDrawSemanticOperandBuilder::build(roundPassageFixture(false)).ok(),"uncertified passage rejected");
    ok&=check(!LDrawSemanticOperandBuilder::build(roundPassageFixture(true,false,false,false,false)).ok(),"similar certified cylinder without reviewed connection evidence rejected");
    LDrawPrintPreparationService syntheticService;PrintPreparationRequest syntheticRequest;syntheticRequest.partReference="synthetic-round";syntheticRequest.ldrawIdentity="synthetic-round";syntheticRequest.libraryAuthority="synthetic";syntheticRequest.loadResult=validFixture;const auto syntheticPrepared=syntheticService.prepare(syntheticRequest);ok&=check(syntheticPrepared.ready(),"synthetic round passage prepares");if(syntheticPrepared.ready()){ok&=check(segmentRemainsOpen(syntheticPrepared.preparedMesh->mesh,{0,0,-2.1},{0,0,2.1}),"round passage remains connected and uncapped");ok&=check(feature&&syntheticPrepared.preparedMesh->functionalFeatures.size()==1&&syntheticPrepared.preparedMesh->functionalFeatures.front().stableIdentity==feature->stableIdentity,"PreparedMesh retains immutable pre-MCUT feature plan");const auto cachedSynthetic=syntheticService.prepare(syntheticRequest);ok&=check(feature&&cachedSynthetic.cacheHit&&cachedSynthetic.preparedMesh->functionalFeatures.front().stableIdentity==feature->stableIdentity,"cache hit preserves functional contract");}

    const auto args=app.arguments();
    const int libraryAt=args.indexOf("--ldraw");
    if(libraryAt<0||libraryAt+1>=args.size())return ok?0:1;

    QString output=QDir::currentPath();
    const int outputAt=args.indexOf("--proof-dir");
    if(outputAt>=0&&outputAt+1<args.size())output=args[outputAt+1];
    QDir().mkpath(output);
    auto cache=std::make_shared<PrintPreparationCache>();
    LDrawPrintPreparationService service(cache);
    for(const QString&id:{QStringLiteral("3003"),QStringLiteral("3001"),QStringLiteral("3622"),QStringLiteral("3700"),QStringLiteral("11477"),QStringLiteral("3673"),QStringLiteral("4274"),QStringLiteral("2780"),QStringLiteral("32064a"),QStringLiteral("3037"),QStringLiteral("6553")}){
        auto loaded=LDrawLibraryService::loadPart(args[libraryAt+1],id);
        ok&=check(loaded.ok(),id+" load");
        if(!loaded.ok())continue;
        const auto semantic=LDrawSemanticOperandBuilder::build(loaded);
        if((id==QStringLiteral("3003")||id==QStringLiteral("3001")||id==QStringLiteral("3700"))&&semantic.ok()){int studCount=0,passageCount=0;for(const auto&operand:semantic.operands)for(const auto&functional:operand.functionalFeatures){if(functional.family==FunctionalInterfaceFamily::RoundTechnicPassage)++passageCount;if(functional.family==FunctionalInterfaceFamily::StandardStud){++studCount;ok&=check(functional.role==FunctionalInterfaceRole::Male&&std::abs(functional.nominalDiameterMillimetres-4.8)<1e-9&&std::abs(functional.nominalAxialExtentMillimetres-1.6)<1e-9&&functional.evidenceContract==QStringLiteral("official-ldraw-standard-stud-v1"),id+" standard stud functional contract");if(id==QStringLiteral("3700"))ok&=check(functional.constructionRecipe==QStringLiteral("standard-open-stud-v1")&&std::abs(functional.protectedInnerRadiusMillimetres-1.6)<1e-9,"3700 recognizes official open studs while protecting their bore");}}const int expectedStuds=id==QStringLiteral("3003")?4:(id==QStringLiteral("3001")?8:2);ok&=check(studCount==expectedStuds,id+" recognizes each reusable ordinary stud instance");if(id==QStringLiteral("3700"))ok&=check(passageCount==1,"3700 recognizes its mixed StandardStud and RoundTechnicPassage families");}
        QTextStream(stdout)<<id
            <<" semanticStatus="<<int(semantic.status)
            <<" groups="<<semantic.semanticGroups
            <<" operands="<<semantic.operands.size()
            <<" sourceTriangles="<<semantic.sourceAnalysis.triangles
            <<" sourceComponents="<<semantic.sourceAnalysis.connectedComponents
            <<" sourceBoundaries="<<semantic.sourceAnalysis.boundaryEdges
            <<" stitchCandidates="<<semantic.stitchDiagnostics.candidateRelationships
            <<" stitchSplits="<<semantic.stitchDiagnostics.acceptedSplits
            <<" stitchTriangles="<<semantic.stitchDiagnostics.trianglesBefore<<"->"<<semantic.stitchDiagnostics.trianglesAfter
            <<" stitchBoundaries="<<semantic.stitchDiagnostics.boundariesBefore<<"->"<<semantic.stitchDiagnostics.boundariesAfter
            <<" diagnostic="<<semantic.diagnostics.join(' ')<<Qt::endl;
        PrintPreparationRequest request;
        request.partReference=id;
        request.ldrawIdentity=id;
        request.libraryAuthority=args[libraryAt+1];
        request.loadResult=loaded;
        auto result=service.prepare(request);
        if(id=="2780"||id=="32064a"||id=="3037"||id=="6553"){ok&=check(!result.ready(),id+" remains safely unsupported");continue;}
        ok&=check(result.ready()&&!result.cacheHit,id+" production preparation");
        if(!result.ready())continue;
        if(id=="11477"){
            ok&=check(semantic.stitchDiagnostics.trianglesBefore==152&&semantic.stitchDiagnostics.trianglesAfter==156,"11477 certified stitching triangle metrics");
            ok&=check(semantic.stitchDiagnostics.boundariesBefore==8&&semantic.stitchDiagnostics.boundariesAfter==0&&semantic.stitchDiagnostics.acceptedSplits==4,"11477 certified stitching closes topology-only seams");
            ok&=check(result.semanticOperandCount==1&&result.operations.isEmpty(),"11477 is one closed semantic body without Boolean reconstruction");
        }
        if(id=="3673"||id=="4274"){
            const auto&mesh=result.preparedMesh->mesh;const auto&bounds=result.finalAnalysis.bounds;const double expectedLength=id=="3673"?16.0:9.6;
            ok&=check(result.preparedMesh->preparationMethod=="Certified authoritative LDraw source-surface volumetric solidification",id+" uses reusable source-surface solidification");
            ok&=check(result.finalAnalysis.boundaryEdges==0&&result.finalAnalysis.nonManifoldEdges==0&&result.finalAnalysis.nonManifoldVertices==0&&result.finalAnalysis.selfIntersections==0&&result.finalAnalysis.connectedComponents==1,id+" is one independently valid printable manifold");
            ok&=check(std::abs(bounds.maximum.x-bounds.minimum.x-expectedLength)<1e-6&&std::abs(bounds.maximum.y-bounds.minimum.y-6.4)<1e-6&&std::abs(bounds.maximum.z-bounds.minimum.z-6.4)<1e-6,id+" preserves authoritative external dimensions");
            ok&=check(segmentRemainsOpen(mesh,{bounds.minimum.x-.1,0,0},{bounds.maximum.x+.1,0,0}),id+" preserves its protected axial bore");
            ok&=check(segmentRemainsOpen(mesh,{-6.0,-3.3,0},{-6.0,3.3,0}),id+" preserves opposed longitudinal slot access to the bore");
            ok&=check(!segmentRemainsOpen(mesh,{bounds.minimum.x+.2,0,1.7},{bounds.minimum.x+.2,0,3.3}),id+" preserves end engagement material away from the intentional slot");
            if(id=="4274"){ok&=check(segmentRemainsOpen(mesh,{.8,-1.5,0},{.8,1.5,0}),"4274 preserves open attachment bore");ok&=check(!segmentRemainsOpen(mesh,{.8,1.7,0},{.8,2.5,0}),"4274 preserves attachment annulus");}
        }
        QString error;
        ok&=check(PreparedObjProofWriter::write(result.preparedMesh->mesh,QDir(output).filePath(id+"-ldraw-aware-proof.obj"),&error),error);
        QElapsedTimer hitTimer;
        hitTimer.start();
        auto cached=service.prepare(request);
        const auto hitMicros=hitTimer.nsecsElapsed()/1000;
        ok&=check(cached.ready()&&cached.cacheHit,id+" cache hit");
        const auto&analysis=result.finalAnalysis;
        if(id=="3700"){ok&=check(segmentRemainsOpen(result.preparedMesh->mesh,{0,-4.1,-4},{0,4.1,-4}),"3700 nominal round passage remains open end-to-end");ok&=check(analysis.triangles==1106&&analysis.connectedComponents==1&&analysis.boundaryEdges==0&&analysis.nonManifoldEdges==0&&analysis.nonManifoldVertices==0&&analysis.selfIntersections==0,"3700 nominal topology remains at committed baseline");const auto d=Point{analysis.bounds.maximum.x-analysis.bounds.minimum.x,analysis.bounds.maximum.y-analysis.bounds.minimum.y,analysis.bounds.maximum.z-analysis.bounds.minimum.z};ok&=check(std::abs(d.x-16.0)<1e-6&&std::abs(d.y-8.0)<1e-6&&std::abs(d.z-11.2005)<1e-3&&std::abs(analysis.absoluteVolume-774.499)<.01,"3700 nominal bounds and volume remain at committed baseline");ok&=check(!result.preparedMesh->functionalFeatures.isEmpty(),"3700 retains functional feature contract");for(const auto&f:result.preparedMesh->functionalFeatures){if(f.family==FunctionalInterfaceFamily::RoundTechnicPassage){ok&=check(f.role==FunctionalInterfaceRole::Female&&f.materialSide==FunctionalMaterialSide::EmptyInsideMaterialOutside&&f.eligibility==FunctionalEligibility::Eligible&&f.operandAction==FunctionalOperandAction::Subtract,"3700 passage is eligible female subtractive clearance");ok&=check(std::abs(f.nominalRadiusMillimetres-2.4)<1e-3&&std::abs(f.nominalDiameterMillimetres-4.8)<2e-3&&std::abs(f.nominalAxialExtentMillimetres-8.0)<1e-6&&std::abs(f.nominalEngagementExtentMillimetres-6.4)<1e-3&&f.radialProfile.size()==6,"3700 governing clearance and stepped entrance profile retained");}QTextStream(stdout)<<"3700 functionalFeature="<<f.stableIdentity<<" radius="<<f.nominalRadiusMillimetres<<" diameter="<<f.nominalDiameterMillimetres<<" extent="<<f.nominalAxialExtentMillimetres<<" engagement="<<f.nominalEngagementExtentMillimetres<<" profileSections="<<f.radialProfile.size()<<" evidence="<<f.evidenceContract<<Qt::endl;}}
        const auto&bounds=analysis.bounds;
        const auto render=PreparedMeshRenderAdapter::fromPreparedMesh(*result.preparedMesh);
        QTextStream(stdout)<<id
            <<" source="<<result.sourceAnalysis.triangles
            <<" sourceComponents="<<result.sourceAnalysis.connectedComponents
            <<" sourceBoundaries="<<result.sourceAnalysis.boundaryEdges
            <<" operands="<<result.semanticOperandCount
            <<" booleans="<<result.operations.size()
            <<" prepared="<<analysis.triangles
            <<" topologyEdges="<<render.topologyEdges.size()
            <<" featureEdges="<<render.featureEdges.size()
            <<" components="<<analysis.connectedComponents
            <<" boundaries="<<analysis.boundaryEdges
            <<" nonManifoldEdges="<<analysis.nonManifoldEdges
            <<" nonManifoldVertices="<<analysis.nonManifoldVertices
            <<" intersections="<<analysis.selfIntersections
            <<" volume="<<analysis.absoluteVolume
            <<" dimensions="<<bounds.maximum.x-bounds.minimum.x<<','<<bounds.maximum.y-bounds.minimum.y<<','<<bounds.maximum.z-bounds.minimum.z
            <<" boundsDeviation="<<result.dimensionalFidelity.maximumBoundsDeviationMillimetres
            <<" sourceMs="<<result.timings.sourceAnalysisMilliseconds
            <<" semanticMs="<<result.timings.semanticConstructionMilliseconds
            <<" operandValidationMs="<<result.timings.operandValidationMilliseconds
            <<" booleanMs="<<result.timings.booleanCompositionMilliseconds
            <<" finalMs="<<result.timings.finalValidationMilliseconds
            <<" totalMs="<<result.timings.totalMilliseconds
            <<" cacheHitUs="<<hitMicros
            <<" cacheBytes="<<cache->statistics().approximateBytes<<Qt::endl;
    }
    return ok?0:1;
}

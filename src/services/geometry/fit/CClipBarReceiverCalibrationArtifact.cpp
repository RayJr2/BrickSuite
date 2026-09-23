#include "CClipBarReceiverCalibrationArtifact.h"

#include "../print/McutMeshBooleanService.h"
#include "../print/PrintMeshAnalysis.h"

#include <cmath>

namespace PrintGeometry { namespace {
constexpr double Pi=3.14159265358979323846;
PrintMesh box(double x0,double x1,double y0,double y1,double z0,double z1) {
    PrintMesh m;
    m.vertices={{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},
                {x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}};
    m.faces={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
             {1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
    return m;
}
PrintMesh compliantClip(double cx,double cy,double innerRadius) {
    // clip6's 5/8-circle contact arc leaves a 3/8-circle compliant mouth.
    // Both the contact radius and the throat chord follow one clearance variable.
    constexpr int segments=40;
    constexpr double outerRadius=2.7;
    constexpr double start=157.5*Pi/180.0;
    constexpr double sweep=225.0*Pi/180.0;
    constexpr double bottom=1.85,top=5.2;
    PrintMesh m;
    for(int i=0;i<=segments;++i) {
        const double a=start+sweep*i/segments;
        const double c=std::cos(a),s=std::sin(a);
        m.vertices.push_back({cx+innerRadius*c,cy+innerRadius*s,bottom});
        m.vertices.push_back({cx+outerRadius*c,cy+outerRadius*s,bottom});
        m.vertices.push_back({cx+innerRadius*c,cy+innerRadius*s,top});
        m.vertices.push_back({cx+outerRadius*c,cy+outerRadius*s,top});
    }
    for(int i=0;i<segments;++i) {
        const auto a=std::uint32_t(i*4),b=a+4;
        m.faces.push_back({a+2,a+3,b+3});m.faces.push_back({a+2,b+3,b+2});
        m.faces.push_back({a,b+1,a+1});m.faces.push_back({a,b,b+1});
        m.faces.push_back({a+1,b+1,b+3});m.faces.push_back({a+1,b+3,a+3});
        m.faces.push_back({a,b+2,b});m.faces.push_back({a,a+2,b+2});
    }
    const auto end=std::uint32_t(segments*4);
    m.faces.push_back({0,1,3});m.faces.push_back({0,3,2});
    m.faces.push_back({end,end+3,end+1});m.faces.push_back({end,end+2,end+3});
    return m;
}
} // namespace

QString CClipBarReceiverCalibrationArtifact::artifactIdentity() {
    return QStringLiteral("c-clip-bar-receiver-clearance-perpendicular-coarse-v1");
}
QString CClipBarReceiverCalibrationArtifact::parallelArtifactIdentity() {
    return QStringLiteral("c-clip-bar-receiver-clearance-parallel-coarse-v1");
}
FunctionalFeature CClipBarReceiverCalibrationArtifact::canonicalPrototype() {
    FunctionalFeature f;
    f.stableIdentity=QStringLiteral("official-clip6-bar-receiver-prototype");
    f.family=FunctionalInterfaceFamily::CClipBarReceiver;
    f.role=FunctionalInterfaceRole::Female;
    f.materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;
    f.eligibility=FunctionalEligibility::Eligible;
    f.confidence=SemanticConfidence::HighConfidence;
    f.frame={{0,0,2},{0,0,1},{1,0,0},{0,1,0},false};
    f.nominalRadiusMillimetres=1.6;
    f.nominalDiameterMillimetres=3.2;
    f.nominalAxialExtentMillimetres=3.2;
    f.nominalEngagementExtentMillimetres=3.2;
    f.operandAction=FunctionalOperandAction::Subtract;
    f.constructionRecipe=QStringLiteral("c-clip-compliant-receiver-v1");
    f.evidenceContract=QStringLiteral("official-ldraw-clip6-bar-receiver-v1");
    f.governingOperandIdentity=f.stableIdentity+QStringLiteral(":inner-arc-and-throat");
    f.radialProfile={{0,1.6},{3.2,1.6}};
    return f;
}
CClipBarReceiverCalibrationResult CClipBarReceiverCalibrationArtifact::generate(
    const CClipBarReceiverCalibrationDefinition& input) {
    CClipBarReceiverCalibrationDefinition d=input;
    const bool parallel=d.orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
    if(d.artifactIdentity.isEmpty())d.artifactIdentity=parallel?parallelArtifactIdentity():artifactIdentity();
    CClipBarReceiverCalibrationResult r;
    r.artifactIdentity=d.artifactIdentity;
    r.parentArtifactIdentity=d.parentArtifactIdentity;
    r.orientationIdentity=parallel?QStringLiteral("flat-base-c-clip-axis-parallel-v1"):
        QStringLiteral("flat-base-c-clip-axis-perpendicular-v1");
    r.regenerationPrototype=canonicalPrototype();
    if((!parallel && d.orientation!=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate)||
       d.candidateCount<3||d.candidateCount>9||d.candidateCount%2==0||
       d.spacingMillimetres<=0||!std::isfinite(d.centerCorrectionMillimetres)) {
        r.diagnostic=QStringLiteral("The C-Clip fixture definition is invalid.");return r;
    }
    constexpr double pitch=8.5,width=8.0;
    McutMeshBooleanService boolean;
    PrintMesh body=box(0,pitch*d.candidateCount,0,width,0,2.0);
    for(int i=0;i<d.candidateCount;++i) {
        const double correction=d.centerCorrectionMillimetres+
            (i-d.candidateCount/2)*d.spacingMillimetres;
        const double diameter=3.2+correction;
        if(diameter<2.8||diameter>3.6) {
            r.diagnostic=QStringLiteral("C-Clip candidate is outside the fixture safety range.");return r;
        }
        PrintMesh clip=compliantClip(pitch*(i+.5),0,diameter*.5);
        if(parallel) {
            // Rigidly rotate the certified contact arc and throat so the clip
            // axis is horizontal while its lower exterior joins the base.
            for(auto& vertex:clip.vertices) {
                const double y=vertex.y,z=vertex.z;
                vertex.y=6.2-z;
                vertex.z=4.4+y;
            }
        } else {
            for(auto& vertex:clip.vertices)vertex.y+=width*.5;
        }
        const auto joined=boolean.unite(body,clip);
        if(!joined.ok()) {
            r.diagnostic=QStringLiteral("C-Clip candidate %1 could not be joined: %2")
                .arg(i+1).arg(QString::fromStdString(joined.message));return r;
        }
        body=joined.mesh;
        FitCalibrationCandidate candidate;
        candidate.index=i+1;
        candidate.diameterCorrectionMillimetres=correction;
        candidate.functionalDiameterMillimetres=diameter;
        r.candidates.push_back(candidate);
    }
    const auto marked=boolean.subtract(body,box(.7,2.7,0,.8,1.0,2.1));
    if(!marked.ok()) {r.diagnostic=QStringLiteral("The Candidate #1-side marker could not be formed.");return r;}
    r.mesh=marked.mesh;
    r.analysis=analyzeSource(r.mesh);
    const auto valid=validatePreparedMesh(r.analysis);
    if(!valid.ok()) {
        r.diagnostic=QStringLiteral("C-Clip fixture failed validation: %1")
            .arg(QString::fromStdString(valid.message));return r;
    }
    r.ok=true;
    r.diagnostic=QStringLiteral("%1 compliant C-Clip contact arcs and linked throats; Candidate #1-side notch.")
        .arg(d.candidateCount);
    return r;
}
FitCalibrationExperiment CClipBarReceiverCalibrationArtifact::observationTemplate(
    const CClipBarReceiverCalibrationResult& r,const CClipBarReceiverCalibrationDefinition& input) {
    FitCalibrationExperiment e;
    e.artifactIdentity=r.artifactIdentity;
    e.parentArtifactIdentity=r.parentArtifactIdentity;
    e.featureFamily=QStringLiteral("CClipBarReceiver");
    e.featureRole=QStringLiteral("female");
    e.modeledOrientationIdentity=r.orientationIdentity;
    e.centerDiameterCorrectionMillimetres=input.centerCorrectionMillimetres;
    e.candidateSpacingMillimetres=input.spacingMillimetres;
    e.regenerationPrototype=r.regenerationPrototype;
    e.hasRegenerationPrototype=true;
    e.candidates=r.candidates;
    e.process.actualPrintedOrientation=input.orientation;
    e.process.orientationNotes=input.orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate
        ? QStringLiteral("Print the flat base down at 100% scale with C-Clip axes parallel to the build plate. Candidate #1 is beside the notch. Snap the same genuine 3.20 mm LEGO bar sideways through each clip throat; test insertion, seated grip, play, removal, stress and repeatability. Do not use a minifigure hand or hinge.")
        : QStringLiteral("Print the flat base down at 100% scale with C-Clip axes vertical. Candidate #1 is beside the notch. Snap the same genuine 3.20 mm LEGO bar sideways through each clip throat; test insertion, seated grip, play, removal, stress and repeatability. Do not use a minifigure hand or hinge.");
    e.process.dimensionalCompensationNotes=QStringLiteral("Record the actual slicer dimensional compensation settings before testing.");
    return e;
}
} // namespace PrintGeometry

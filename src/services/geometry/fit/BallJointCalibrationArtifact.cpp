#include "BallJointCalibrationArtifact.h"

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
PrintMesh ballOnStem(double cx,double cy,double radius) {
    constexpr int segments=48,latitude=18;
    constexpr double stemRadius=1.6,centerHeight=7.0;
    const double lowerAngle=std::acos(-std::sqrt(radius*radius-stemRadius*stemRadius)/radius);
    PrintMesh m;
    const auto ring=[&](double r,double height) {
        for(int i=0;i<segments;++i) {
            const double angle=2*Pi*i/segments;
            m.vertices.push_back({cx+r*std::cos(angle),cy+r*std::sin(angle),height});
        }
    };
    ring(stemRadius,1.85);
    ring(stemRadius,centerHeight+radius*std::cos(lowerAngle));
    for(int j=1;j<latitude;++j) {
        const double angle=lowerAngle*(1.0-double(j)/latitude);
        ring(radius*std::sin(angle),centerHeight+radius*std::cos(angle));
    }
    const int rings=latitude+1;
    for(int j=0;j<rings-1;++j)for(int i=0;i<segments;++i) {
        const auto a=std::uint32_t(j*segments+i),b=std::uint32_t(j*segments+(i+1)%segments);
        const auto c=std::uint32_t((j+1)*segments+i),d=std::uint32_t((j+1)*segments+(i+1)%segments);
        m.faces.push_back({a,b,c});m.faces.push_back({b,d,c});
    }
    const auto bottom=std::uint32_t(m.vertices.size());m.vertices.push_back({cx,cy,1.85});
    const auto top=std::uint32_t(m.vertices.size());m.vertices.push_back({cx,cy,centerHeight+radius});
    for(int i=0;i<segments;++i) {
        const auto next=std::uint32_t((i+1)%segments);
        m.faces.push_back({bottom,next,std::uint32_t(i)});
        const auto a=std::uint32_t((rings-1)*segments+i),b=std::uint32_t((rings-1)*segments+(i+1)%segments);
        m.faces.push_back({a,b,top});
    }
    return m;
}
} // namespace

QString BallJointCalibrationArtifact::artifactIdentity() {
    return QStringLiteral("ball-joint-diameter-perpendicular-coarse-v1");
}
FunctionalFeature BallJointCalibrationArtifact::canonicalPrototype() {
    FunctionalFeature f;
    f.stableIdentity=QStringLiteral("official-joint8ball-sphere-prototype");
    f.family=FunctionalInterfaceFamily::BallJoint;
    f.role=FunctionalInterfaceRole::Male;
    f.materialSide=FunctionalMaterialSide::MaterialInside;
    f.eligibility=FunctionalEligibility::Eligible;
    f.confidence=SemanticConfidence::HighConfidence;
    f.frame={{0,0,7},{0,0,1},{1,0,0},{0,1,0},false};
    f.nominalRadiusMillimetres=3.2;
    f.nominalDiameterMillimetres=6.4;
    f.nominalAxialExtentMillimetres=6.4;
    f.nominalEngagementExtentMillimetres=6.4;
    f.operandAction=FunctionalOperandAction::Unite;
    f.constructionRecipe=QStringLiteral("ball-joint-8-spherical-head-v1");
    f.evidenceContract=QStringLiteral("official-ldraw-joint8ball-sphere-v1");
    f.governingOperandIdentity=f.stableIdentity+QStringLiteral(":spherical-head");
    f.radialProfile={{-3.2,0},{0,3.2},{3.2,0}};
    return f;
}
BallJointCalibrationResult BallJointCalibrationArtifact::generate(const BallJointCalibrationDefinition& input) {
    BallJointCalibrationDefinition d=input;
    if(d.artifactIdentity.isEmpty())d.artifactIdentity=artifactIdentity();
    BallJointCalibrationResult r;
    r.artifactIdentity=d.artifactIdentity;
    r.parentArtifactIdentity=d.parentArtifactIdentity;
    r.orientationIdentity=QStringLiteral("flat-base-ball-axis-perpendicular-v1");
    r.regenerationPrototype=canonicalPrototype();
    if(d.candidateCount<3||d.candidateCount>9||d.candidateCount%2==0||
       d.spacingMillimetres<=0||!std::isfinite(d.centerCorrectionMillimetres)) {
        r.diagnostic=QStringLiteral("The Ball Joint fixture definition is invalid.");return r;
    }
    constexpr double pitch=9.0,width=9.0;
    McutMeshBooleanService boolean;
    PrintMesh body=box(0,pitch*d.candidateCount,0,width,0,2.0);
    for(int i=0;i<d.candidateCount;++i) {
        const double correction=d.centerCorrectionMillimetres+(i-d.candidateCount/2)*d.spacingMillimetres;
        const double diameter=6.4+correction;
        if(diameter<5.8||diameter>7.0) {
            r.diagnostic=QStringLiteral("Ball Joint candidate is outside the fixture safety range.");return r;
        }
        auto joined=boolean.unite(body,ballOnStem(pitch*(i+.5),width*.5,diameter*.5));
        if(!joined.ok()) {
            r.diagnostic=QStringLiteral("Ball Joint candidate %1 could not be joined: %2")
                .arg(i+1).arg(QString::fromStdString(joined.message));return r;
        }
        body=std::move(joined.mesh);
        r.candidates.push_back({i+1,correction,diameter});
    }
    const auto marked=boolean.subtract(body,box(.7,2.7,0,.8,1.0,2.1));
    if(!marked.ok()) {r.diagnostic=QStringLiteral("The Candidate #1-side marker could not be formed.");return r;}
    r.mesh=marked.mesh;
    r.analysis=analyzeSource(r.mesh);
    const auto valid=validatePreparedMesh(r.analysis);
    if(!valid.ok()) {
        r.diagnostic=QStringLiteral("Ball Joint fixture failed validation: %1")
            .arg(QString::fromStdString(valid.message));return r;
    }
    r.ok=true;
    r.diagnostic=QStringLiteral("%1 independent spherical male Ball Joint heads on 3.20 mm stems; Candidate #1-side notch.").arg(d.candidateCount);
    return r;
}
FitCalibrationExperiment BallJointCalibrationArtifact::observationTemplate(
    const BallJointCalibrationResult& r,const BallJointCalibrationDefinition& input) {
    FitCalibrationExperiment e;
    e.artifactIdentity=r.artifactIdentity;
    e.parentArtifactIdentity=r.parentArtifactIdentity;
    e.featureFamily=QStringLiteral("BallJoint");
    e.featureRole=QStringLiteral("male");
    e.modeledOrientationIdentity=r.orientationIdentity;
    e.centerDiameterCorrectionMillimetres=input.centerCorrectionMillimetres;
    e.candidateSpacingMillimetres=input.spacingMillimetres;
    e.regenerationPrototype=r.regenerationPrototype;
    e.hasRegenerationPrototype=true;
    e.candidates=r.candidates;
    e.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
    e.process.orientationNotes=QStringLiteral("Print the flat base down at 100% scale with ball stems vertical. Candidate #1 is beside the notch. Test each ball in the same genuine LEGO joint8 socket, recording snap-in, articulation, retention, removal, stress and repeatability. Do not use another ball-joint class or decorative rounded cavity.");
    e.process.dimensionalCompensationNotes=QStringLiteral("Record the actual slicer dimensional compensation settings before testing.");
    return e;
}
} // namespace PrintGeometry

#include "StandardBarCalibrationArtifact.h"

#include "../print/McutMeshBooleanService.h"
#include "../print/PrintMeshAnalysis.h"

#include <cmath>

namespace PrintGeometry { namespace {
PrintMesh box(double x0,double x1,double y0,double y1,double z0,double z1) {
    PrintMesh m;
    m.vertices={{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},
                {x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}};
    m.faces={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
             {1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
    return m;
}
PrintMesh cylinder(double cx,double cy,double radius,double bottom,double top) {
    constexpr int segments=48;
    constexpr double pi=3.14159265358979323846;
    PrintMesh mesh;
    for(int ring=0;ring<2;++ring) for(int i=0;i<segments;++i) {
        const double a=2*pi*i/segments;
        mesh.vertices.push_back({cx+radius*std::cos(a),cy+radius*std::sin(a),ring?top:bottom});
    }
    const auto bottomCenter=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back({cx,cy,bottom});
    const auto topCenter=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back({cx,cy,top});
    for(int i=0;i<segments;++i) {
        const auto j=(i+1)%segments;
        mesh.faces.push_back({std::uint32_t(i),std::uint32_t(j),std::uint32_t(segments+i)});
        mesh.faces.push_back({std::uint32_t(j),std::uint32_t(segments+j),std::uint32_t(segments+i)});
        mesh.faces.push_back({bottomCenter,std::uint32_t(j),std::uint32_t(i)});
        mesh.faces.push_back({topCenter,std::uint32_t(segments+i),std::uint32_t(segments+j)});
    }
    return mesh;
}
} // namespace

QString StandardBarCalibrationArtifact::artifactIdentity() {
    return QStringLiteral("standard-bar-diameter-perpendicular-coarse-v1");
}
FunctionalFeature StandardBarCalibrationArtifact::canonicalPrototype() {
    FunctionalFeature f;
    f.stableIdentity=QStringLiteral("official-capped-standard-bar-prototype");
    f.family=FunctionalInterfaceFamily::StandardBar;
    f.role=FunctionalInterfaceRole::Male;
    f.materialSide=FunctionalMaterialSide::MaterialInside;
    f.eligibility=FunctionalEligibility::Eligible;
    f.confidence=SemanticConfidence::HighConfidence;
    f.frame={{0,0,2},{0,0,1},{1,0,0},{0,1,0},false};
    f.nominalRadiusMillimetres=1.6;
    f.nominalDiameterMillimetres=3.2;
    f.nominalAxialExtentMillimetres=8.0;
    f.nominalEngagementExtentMillimetres=8.0;
    f.operandAction=FunctionalOperandAction::Unite;
    f.constructionRecipe=QStringLiteral("standard-bar-simple-cylinder-v1");
    f.evidenceContract=QStringLiteral("official-ldraw-capped-standard-bar-v1");
    f.governingOperandIdentity=f.stableIdentity+QStringLiteral(":body-cylinder");
    f.radialProfile={{0,1.6},{8.0,1.6}};
    return f;
}
StandardBarCalibrationResult StandardBarCalibrationArtifact::generate(const StandardBarCalibrationDefinition& input) {
    StandardBarCalibrationDefinition d=input;
    if(d.artifactIdentity.isEmpty()) d.artifactIdentity=artifactIdentity();
    StandardBarCalibrationResult r;
    r.artifactIdentity=d.artifactIdentity;
    r.parentArtifactIdentity=d.parentArtifactIdentity;
    r.orientationIdentity=QStringLiteral("flat-base-standard-bar-axis-perpendicular-v1");
    r.regenerationPrototype=canonicalPrototype();
    if(d.candidateCount<3 || d.candidateCount>9 || d.candidateCount%2==0 ||
       d.spacingMillimetres<=0 || !std::isfinite(d.centerCorrectionMillimetres)) {
        r.diagnostic=QStringLiteral("The Standard Bar fixture definition is invalid.");
        return r;
    }
    constexpr double pitch=9.0,width=9.0;
    McutMeshBooleanService boolean;
    PrintMesh body=box(0,pitch*d.candidateCount,0,width,0,2.0);
    for(int i=0;i<d.candidateCount;++i) {
        const double correction=d.centerCorrectionMillimetres+(i-d.candidateCount/2)*d.spacingMillimetres;
        const double diameter=3.2+correction;
        if(diameter<2.7 || diameter>3.7) {
            r.diagnostic=QStringLiteral("Standard Bar candidate is outside the fixture safety range.");
            return r;
        }
        auto joined=boolean.unite(body,cylinder(pitch*(i+.5),width*.5,diameter*.5,1.9,10.0));
        if(!joined.ok()) {
            r.diagnostic=QStringLiteral("Standard Bar candidate %1 could not be joined: %2")
                .arg(i+1).arg(QString::fromStdString(joined.message));
            return r;
        }
        body=std::move(joined.mesh);
        FitCalibrationCandidate c;
        c.index=i+1;
        c.diameterCorrectionMillimetres=correction;
        c.functionalDiameterMillimetres=diameter;
        r.candidates.push_back(c);
    }
    auto marked=boolean.subtract(body,box(.7,2.7,0,.8,1.0,2.1));
    if(!marked.ok()) {
        r.diagnostic=QStringLiteral("The Candidate #1-side marker could not be formed.");
        return r;
    }
    r.mesh=std::move(marked.mesh);
    r.analysis=analyzeSource(r.mesh);
    const auto valid=validatePreparedMesh(r.analysis);
    if(!valid.ok()) {
        r.diagnostic=QStringLiteral("Standard Bar fixture failed validation: %1")
            .arg(QString::fromStdString(valid.message));
        return r;
    }
    r.ok=true;
    r.diagnostic=QStringLiteral("%1 independent male Standard Bar diameters, 8.00 mm free engagement, Candidate #1-side notch.")
        .arg(d.candidateCount);
    return r;
}
FitCalibrationExperiment StandardBarCalibrationArtifact::observationTemplate(
    const StandardBarCalibrationResult& r,const StandardBarCalibrationDefinition& input) {
    FitCalibrationExperiment e;
    e.artifactIdentity=r.artifactIdentity;
    e.parentArtifactIdentity=r.parentArtifactIdentity;
    e.featureFamily=QStringLiteral("StandardBar");
    e.featureRole=QStringLiteral("male");
    e.modeledOrientationIdentity=r.orientationIdentity;
    e.centerDiameterCorrectionMillimetres=input.centerCorrectionMillimetres;
    e.candidateSpacingMillimetres=input.spacingMillimetres;
    e.regenerationPrototype=r.regenerationPrototype;
    e.hasRegenerationPrototype=true;
    e.candidates=r.candidates;
    e.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
    e.process.orientationNotes=QStringLiteral("Print the flat base down with bars vertical. Candidate #1 is beside the side notch. Test each candidate in the same genuine LEGO standard-bar grip or clip; do not use a minifigure hand or Technic hole.");
    e.process.dimensionalCompensationNotes=QStringLiteral("Print at 100% scale and record slicer dimensional compensation settings.");
    return e;
}
} // namespace PrintGeometry

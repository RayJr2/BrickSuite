#include "PinBarrelHingeCalibrationArtifact.h"

#include "../print/PinBarrelHingeSemantic.h"
#include "../print/McutMeshBooleanService.h"
#include "../print/PrintMeshAnalysis.h"
#include "../print/SourceSurfaceSolidifier.h"

#include <QHash>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point converted(const QVector3D& p) { return {double(p.x())*MmPerLdu,double(p.z())*MmPerLdu,-double(p.y())*MmPerLdu}; }
QVector3D sourcePoint(Point p) { return {float(p.x/MmPerLdu),float(-p.z/MmPerLdu),float(p.y/MmPerLdu)}; }
Point add(Point a,Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Point subtract(Point a,Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Point scaled(Point a,double s) { return {a.x*s,a.y*s,a.z*s}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Point a) { return std::sqrt(dot(a,a)); }
QString key(Point p) { return QStringLiteral("%1|%2|%3").arg(std::llround(p.x*100000.0))
    .arg(std::llround(p.y*100000.0)).arg(std::llround(p.z*100000.0)); }
bool descendant(const LDrawGeometry::LDrawSourceModel& model,int reference,int ancestor) {
    while(reference>=0&&reference<model.references.size()&&reference!=ancestor)
        reference=model.references[reference].parentId;
    return reference==ancestor;
}
struct Movement { Point sum;int count=0; };
PrintMesh box(double x0,double x1,double y0,double y1,double z0,double z1) {
    PrintMesh m;
    m.vertices={{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},
                {x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}};
    m.faces={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
             {1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
    return m;
}
std::vector<PrintMesh> numeral(int number) {
    static constexpr int masks[7]={0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07};
    const int mask=masks[number-1];
    std::vector<PrintMesh> strokes;
    auto stroke=[&](int bit,double x0,double x1,double z0,double z1) {
        if(mask&(1<<bit))strokes.push_back(box(x0,x1,4.22,4.58,z0,z1));
    };
    stroke(0,-.9,.9,-1.65,-1.27);
    stroke(1,.52,.9,-1.43,-.18);
    stroke(2,.52,.9,.05,1.30);
    stroke(3,-.9,.9,1.08,1.46);
    stroke(4,-.9,-.52,.05,1.30);
    stroke(5,-.9,-.52,-1.43,-.18);
    stroke(6,-.9,.9,-.18,.20);
    return strokes;
}
} // namespace

QString PinBarrelHingeCalibrationArtifact::artifactIdentity() {
    return QStringLiteral("pin-barrel-hinge-male-pin-diameter-parallel-coarse-v1");
}

PinBarrelHingeCalibrationResult PinBarrelHingeCalibrationArtifact::generate(
    const LDrawGeometry::LDrawLoadResult& source,const PinBarrelHingeCalibrationDefinition& input) {
    PinBarrelHingeCalibrationResult result;
    result.artifactIdentity=input.artifactIdentity.isEmpty()?artifactIdentity():input.artifactIdentity;
    result.parentArtifactIdentity=input.parentArtifactIdentity;
    result.orientationIdentity=QStringLiteral("3938-long-side-down-hinge-axis-parallel-v1");
    if(!source.ok()||!source.sourceModel||
       input.orientation!=FitPrintedOrientation::FeatureAxisParallelToBuildPlate||
       input.candidateCount!=7||!std::isfinite(input.centerCorrectionMillimetres)||
       !std::isfinite(input.spacingMillimetres)||input.spacingMillimetres<=0) {
        result.diagnostic=QStringLiteral("The certified pin/barrel hinge fixture definition is invalid.");return result;
    }
    const auto pins=PinBarrelHingeSemantic::recognize(source);
    if(pins.size()!=2||pins[0].role!=FunctionalInterfaceRole::Male||
       pins[1].role!=FunctionalInterfaceRole::Male) {
        result.diagnostic=QStringLiteral("The source must be the certified two-pin 3938 hinge top.");return result;
    }
    result.regenerationPrototype=pins.front();
    auto& printedFrame=result.regenerationPrototype.frame;
    printedFrame.origin={pins.front().frame.origin.x,-pins.front().frame.origin.z,
                         pins.front().frame.origin.y+4.0};
    printedFrame.axis={pins.front().frame.axis.x,-pins.front().frame.axis.z,pins.front().frame.axis.y};
    printedFrame.profileU={pins.front().frame.profileU.x,-pins.front().frame.profileU.z,pins.front().frame.profileU.y};
    printedFrame.profileV={pins.front().frame.profileV.x,-pins.front().frame.profileV.z,pins.front().frame.profileV.y};
    const auto& model=*source.sourceModel;
    QHash<QString,Movement> movements;
    int ownedOuterFaces=0;
    for(const auto& pin:pins) {
        const int owner=pin.provenance.back().referenceId;
        for(const auto& surface:model.surfaces) {
            if(!descendant(model,surface.referenceId,owner)||!surface.certified||
               surface.triangleIndex<0||surface.triangleIndex>=source.mesh.triangles.size())continue;
            ++ownedOuterFaces;
            const auto& triangle=source.mesh.triangles[surface.triangleIndex];
            for(const auto& vertex:{triangle.a,triangle.b,triangle.c}) {
                const Point p=converted(vertex),relative=subtract(p,pin.frame.origin);
                const double axial=dot(relative,pin.frame.axis);
                const Point transverse=subtract(relative,scaled(pin.frame.axis,axial));
                const double radius=length(transverse);
                if(axial<-.001||axial>1.601||std::abs(radius-1.6)>.002)continue;
                auto& movement=movements[key(p)];
                movement.sum=add(movement.sum,scaled(transverse,1.0/radius));
                ++movement.count;
            }
        }
    }
    if(ownedOuterFaces<32||movements.size()<32) {
        result.diagnostic=QStringLiteral("The certified pin outer surfaces are incomplete.");return result;
    }
    for(int i=0;i<input.candidateCount;++i) {
        const double correction=input.centerCorrectionMillimetres+
            (i-input.candidateCount/2)*input.spacingMillimetres;
        if(std::abs(correction)>.25||3.2+correction<=1.6) {
            result.diagnostic=QStringLiteral("The candidate exceeds the safe hinge pin range.");return result;
        }
        auto candidate=source;
        if(correction!=0)for(auto& triangle:candidate.mesh.triangles)
            for(auto* vertex:{&triangle.a,&triangle.b,&triangle.c}) {
                const Point p=converted(*vertex);
                const auto movement=movements.constFind(key(p));
                if(movement==movements.cend())continue;
                const Point direction=scaled(movement->sum,1.0/movement->count);
                *vertex=sourcePoint(add(p,scaled(direction,correction*.5)));
            }
        auto solidified=SourceSurfaceSolidifier::solidify(candidate);
        if(!solidified.successful||!validatePreparedMesh(solidified.analysis).ok()) {
            result.diagnostic=QStringLiteral("Hinge candidate %1 failed source-faithful solidification: %2")
                .arg(i+1).arg(solidified.diagnostic);return result;
        }
        PrintMesh prepared=std::move(solidified.mesh);
        // The central exterior tab and number are outside both pin contact
        // surfaces, their protected bores, and the mating barrel shoulders.
        McutMeshBooleanService boolean;
        PrintMesh label=box(-1.35,1.35,3.73,4.28,-2.0,1.8);
        for(const auto& stroke:numeral(i+1)) {
            const auto joined=boolean.unite(label,stroke);
            if(!joined.ok()||!validatePreparedMesh(joined.resultAnalysis).ok()) {
                result.diagnostic=QStringLiteral("Candidate %1 numeral could not be joined.").arg(i+1);return result;
            }
            label=joined.mesh;
        }
        const auto marked=boolean.unite(prepared,label);
        if(!marked.ok()||!validatePreparedMesh(marked.resultAnalysis).ok()) {
            result.diagnostic=QStringLiteral("Candidate %1 label tab could not be joined: %2")
                .arg(i+1).arg(QString::fromStdString(marked.message));return result;
        }
        prepared=marked.mesh;
        // The long outer plate side lies on the bed. Both pin axes remain
        // horizontal, elevated clear of the bed, with no support on contact.
        for(auto& p:prepared.vertices) {
            const double y=p.y,z=p.z;
            p.y=-z;
            p.z=y+4.0;
        }
        const auto analysis=analyzeSource(prepared);
        if(!validatePreparedMesh(analysis).ok()||analysis.bounds.minimum.z<-.05) {
            result.diagnostic=QStringLiteral("Candidate %1 is not a valid side-down printable solid.").arg(i+1);return result;
        }
        result.candidateMeshes.push_back(std::move(prepared));
        result.candidates.push_back({i+1,correction,3.2+correction});
    }
    result.ok=true;
    result.diagnostic=QStringLiteral("Seven source-owned hollow-pin candidates; %1 certified outer-wall triangles govern OD while the bore and barrel stay nominal.")
        .arg(ownedOuterFaces);
    return result;
}

FitCalibrationExperiment PinBarrelHingeCalibrationArtifact::observationTemplate(
    const PinBarrelHingeCalibrationResult& result,const PinBarrelHingeCalibrationDefinition& input) {
    FitCalibrationExperiment e;
    e.artifactIdentity=result.artifactIdentity;
    e.parentArtifactIdentity=result.parentArtifactIdentity;
    e.featureFamily=QStringLiteral("PinBarrelHinge");
    e.featureRole=QStringLiteral("male");
    e.modeledOrientationIdentity=result.orientationIdentity;
    e.centerDiameterCorrectionMillimetres=input.centerCorrectionMillimetres;
    e.candidateSpacingMillimetres=input.spacingMillimetres;
    e.regenerationPrototype=result.regenerationPrototype;
    e.hasRegenerationPrototype=true;
    e.candidates=result.candidates;
    e.process.actualPrintedOrientation=input.orientation;
    e.process.orientationNotes=QStringLiteral("Print each numbered 3938 hinge top on its long exterior plate side at 100% scale, with both pin axes parallel to the build plate. Keep supports away from the two pin cylinders and protected bores. Snap each candidate into the same genuine LEGO 3937 base repeatedly; record assembly force, smooth rotation, retained alignment, binding, play, removal force, wear and repeatability. Candidate #4 is nominal. This first contract varies only the paired male pin OD; the real barrel remains the fixed reference.");
    e.process.dimensionalCompensationNotes=QStringLiteral("Record actual slicer dimensional compensation settings before testing.");
    return e;
}
}

#include "BallSocketCalibrationArtifact.h"

#include "../print/BallSocketSemantic.h"
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
Point scaled(Point a,double value) { return {a.x*value,a.y*value,a.z*value}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Point a) { return std::sqrt(dot(a,a)); }
Point cross(Point a,Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
QString key(Point p) { return QStringLiteral("%1|%2|%3").arg(std::llround(p.x*100000.0)).arg(std::llround(p.y*100000.0)).arg(std::llround(p.z*100000.0)); }
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
std::vector<PrintMesh> candidateNumber(int number) {
    // Seven-segment numeral, embossed on the exterior plate-side pad.
    // Coordinates are source/pre-print-rotation: +y becomes the printed top.
    static constexpr int segments[7]={0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07};
    const int mask=segments[number-1];
    std::vector<PrintMesh> numeral;
    auto stroke=[&](int bit,double x0,double x1,double z0,double z1) {
        if(mask&(1<<bit))numeral.push_back(box(x0,x1,4.12,4.58,z0,z1));
    };
    stroke(0,-6.70,-5.10,-2.95,-2.50); // top
    stroke(1,-5.55,-5.10,-2.68,-1.74); // upper right
    stroke(2,-5.55,-5.10,-1.52,-.58); // lower right
    stroke(3,-6.70,-5.10,-.85,-.40); // bottom
    stroke(4,-6.70,-6.25,-1.52,-.58); // lower left
    stroke(5,-6.70,-6.25,-2.68,-1.74); // upper left
    stroke(6,-6.70,-5.10,-1.88,-1.43); // middle
    return numeral;
}
void contribute(QHash<QString,Movement>* movements,Point point,Point direction) {
    auto& movement=(*movements)[key(point)];
    movement.sum=add(movement.sum,direction);
    ++movement.count;
}
} // namespace

QString BallSocketCalibrationArtifact::artifactIdentity() {
    return QStringLiteral("ball-socket-friction-contact-throat-parallel-coarse-v1");
}

BallSocketCalibrationResult BallSocketCalibrationArtifact::generate(
    const LDrawGeometry::LDrawLoadResult& source,const BallSocketCalibrationDefinition& input) {
    BallSocketCalibrationResult result;
    result.artifactIdentity=input.artifactIdentity.isEmpty()?artifactIdentity():input.artifactIdentity;
    result.parentArtifactIdentity=input.parentArtifactIdentity;
    result.orientationIdentity=QStringLiteral("flat-base-ball-socket-mouth-axis-parallel-v1");
    if(!source.ok()||!source.sourceModel||input.orientation!=FitPrintedOrientation::FeatureAxisParallelToBuildPlate||
       input.candidateCount<3||input.candidateCount>9||input.candidateCount%2==0||
       !std::isfinite(input.centerCorrectionMillimetres)||!std::isfinite(input.spacingMillimetres)||
       input.spacingMillimetres<=0) {
        result.diagnostic=QStringLiteral("The certified Ball Socket fixture definition is invalid.");return result;
    }
    const auto sockets=BallSocketSemantic::recognize(source);
    if(sockets.size()!=1||sockets.front().provenance.size()!=2||
       std::abs(sockets.front().frame.axis.z)>.1) {
        result.diagnostic=QStringLiteral("The source must contain one certified friction socket with its mouth axis parallel to the print bed.");return result;
    }
    const auto& socket=sockets.front();
    result.regenerationPrototype=socket;
    auto& printedFrame=result.regenerationPrototype.frame;
    printedFrame.origin={socket.frame.origin.x,-socket.frame.origin.z,socket.frame.origin.y+4.0};
    printedFrame.axis={socket.frame.axis.x,-socket.frame.axis.z,socket.frame.axis.y};
    printedFrame.profileU={socket.frame.profileU.x,-socket.frame.profileU.z,socket.frame.profileU.y};
    printedFrame.profileV={socket.frame.profileV.x,-socket.frame.profileV.z,socket.frame.profileV.y};
    const auto& model=*source.sourceModel;
    const int owner=socket.provenance.back().referenceId;
    QHash<QString,Movement> movements;
    int contactFaces=0,mouthVertices=0;
    for(const auto& surface:model.surfaces) {
        if(!descendant(model,surface.referenceId,owner)||!surface.certified||
           surface.triangleIndex<0||surface.triangleIndex>=source.mesh.triangles.size())continue;
        const auto& triangle=source.mesh.triangles[surface.triangleIndex];
        const Point p[3]={converted(triangle.a),converted(triangle.b),converted(triangle.c)};
        const Point centroid=scaled(add(add(p[0],p[1]),p[2]),1.0/3.0);
        const Point fromCenter=subtract(centroid,socket.frame.origin);
        const Point normal=cross(subtract(p[1],p[0]),subtract(p[2],p[0]));
        const bool sphericalContact=length(fromCenter)>=3.04&&length(fromCenter)<=3.25&&
            dot(normal,fromCenter)<0;
        if(sphericalContact)++contactFaces;
        for(const auto& point:p) {
            const Point from=subtract(point,socket.frame.origin);
            const double axial=dot(from,socket.frame.axis);
            const Point transverse=subtract(from,scaled(socket.frame.axis,axial));
            const double transverseRadius=length(transverse);
            const bool retainingMouth=axial>=1.55&&axial<=2.7&&
                transverseRadius>=2.35&&transverseRadius<=3.35;
            if(retainingMouth)++mouthVertices;
            if(retainingMouth)
                contribute(&movements,point,scaled(transverse,1.0/transverseRadius));
            else if(sphericalContact&&length(from)>1e-6)
                contribute(&movements,point,scaled(from,1.0/length(from)));
        }
    }
    if(contactFaces<40||mouthVertices<12||movements.size()<40) {
        result.diagnostic=QStringLiteral("Certified contact or retaining-mouth source ownership is incomplete (%1 faces, %2 mouth vertices).")
            .arg(contactFaces).arg(mouthVertices);return result;
    }
    for(int i=0;i<input.candidateCount;++i) {
        const double correction=input.centerCorrectionMillimetres+
            (i-input.candidateCount/2)*input.spacingMillimetres;
        if(std::abs(correction)>.4) {
            result.diagnostic=QStringLiteral("The coupled Ball Socket contact/throat offset exceeds the safe coarse range.");return result;
        }
        auto candidate=source;
        if(correction!=0)for(auto& triangle:candidate.mesh.triangles)
            for(auto* vertex:{&triangle.a,&triangle.b,&triangle.c}) {
                const Point point=converted(*vertex);
                const auto it=movements.constFind(key(point));
                if(it==movements.cend())continue;
                const auto direction=scaled(it->sum,1.0/it->count);
                *vertex=sourcePoint(add(point,scaled(direction,correction*.5)));
            }
        auto solidified=SourceSurfaceSolidifier::solidify(candidate);
        if(!solidified.successful||!validatePreparedMesh(solidified.analysis).ok()) {
            result.diagnostic=QStringLiteral("Ball Socket candidate %1 could not be solidified without topology loss: %2")
                .arg(i+1).arg(solidified.diagnostic);return result;
        }
        PrintMesh prepared=std::move(solidified.mesh);
        // Every tile has an embossed numeral on the same non-fit exterior.
        // The pad and strokes are added after source-faithful solidification;
        // neither the socket contact nor the retaining throat is regenerated.
        McutMeshBooleanService boolean;
        PrintMesh label=box(-7.05,-4.75,3.78,4.24,-3.12,-.18);
        for(const auto& stroke:candidateNumber(i+1)) {
            const auto labeled=boolean.unite(label,stroke);
            if(!labeled.ok()||!validatePreparedMesh(labeled.resultAnalysis).ok()) {
                result.diagnostic=QStringLiteral("Candidate %1 numeral could not be joined: %2")
                    .arg(i+1).arg(QString::fromStdString(labeled.message));return result;
            }
            label=labeled.mesh;
        }
        const auto marked=boolean.unite(prepared,label);
        if(!marked.ok()||!validatePreparedMesh(marked.resultAnalysis).ok()) {
            result.diagnostic=QStringLiteral("Candidate %1 numbered exterior could not be joined: %2")
                .arg(i+1).arg(QString::fromStdString(marked.message));return result;
        }
        prepared=marked.mesh;
        // Rest a certified flat plate side on the bed. The socket mouth axis
        // stays horizontal and the contact/throat surfaces stay off the bed.
        for(auto& point:prepared.vertices) {
            const double y=point.y,z=point.z;
            point.y=-z;
            point.z=y+4.0;
        }
        const auto oriented=analyzeSource(prepared);
        if(!validatePreparedMesh(oriented).ok()||oriented.bounds.minimum.z<-.05) {
            result.diagnostic=QStringLiteral("The side-down Ball Socket candidate is not a valid printable solid.");return result;
        }
        result.candidateMeshes.push_back(std::move(prepared));
        result.candidates.push_back({i+1,correction,6.4+correction});
    }
    result.ok=true;
    result.diagnostic=QStringLiteral("%1 certified friction-socket contacts and retaining mouths regenerated; %2 source contact faces and %3 mouth vertices govern the coupled offset.")
        .arg(input.candidateCount).arg(contactFaces).arg(mouthVertices);
    return result;
}

FitCalibrationExperiment BallSocketCalibrationArtifact::observationTemplate(
    const BallSocketCalibrationResult& result,const BallSocketCalibrationDefinition& input) {
    FitCalibrationExperiment experiment;
    experiment.artifactIdentity=result.artifactIdentity;
    experiment.parentArtifactIdentity=result.parentArtifactIdentity;
    experiment.featureFamily=QStringLiteral("BallSocket");
    experiment.featureRole=QStringLiteral("female");
    experiment.modeledOrientationIdentity=result.orientationIdentity;
    experiment.centerDiameterCorrectionMillimetres=input.centerCorrectionMillimetres;
    experiment.candidateSpacingMillimetres=input.spacingMillimetres;
    experiment.regenerationPrototype=result.regenerationPrototype;
    experiment.hasRegenerationPrototype=true;
    experiment.candidates=result.candidates;
    experiment.process.actualPrintedOrientation=input.orientation;
    experiment.process.orientationNotes=QStringLiteral("Print each tile on its smooth long plate side at 100% scale, with the certified friction-socket mouth axis parallel to the build plate and the socket above the bed. Each tile has an embossed exterior candidate number. Keep supports off the socket contact and retaining mouth. Test every candidate with the same genuine 6.40 mm LEGO joint8 ball; record snap-in, seated play, articulation, retention, removal, stress and repeatability. Do not use the free-moving joint8socket2 type.");
    experiment.process.dimensionalCompensationNotes=QStringLiteral("Record the actual slicer dimensional compensation settings before testing.");
    return experiment;
}
} // namespace PrintGeometry

#include "RetainedRotatingWheelCalibrationArtifact.h"
#include "HingeCalibrationDotMarker.h"
#include "../print/RetainedRotatingWheelSemantic.h"
#include "../print/McutMeshBooleanService.h"
#include "../print/PrintMeshAnalysis.h"
#include "../print/SourceSurfaceSolidifier.h"

#include <algorithm>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point converted(const QVector3D& p){return {double(p.x())*MmPerLdu,double(p.z())*MmPerLdu,-double(p.y())*MmPerLdu};}
QVector3D sourcePoint(Point p){return {float(p.x/MmPerLdu),float(-p.z/MmPerLdu),float(p.y/MmPerLdu)};}
Point subtract(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point add(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Point scaled(Point a,double s){return {a.x*s,a.y*s,a.z*s};}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double length(Point a){return std::sqrt(dot(a,a));}
} // namespace

QString RetainedRotatingWheelCalibrationArtifact::artifactIdentity(){
    return QStringLiteral("retained-wheel-wpin2a-wpinhol2-bearing-perpendicular-coarse-v1");
}

RetainedRotatingWheelCalibrationResult RetainedRotatingWheelCalibrationArtifact::generate(
    const LDrawGeometry::LDrawLoadResult& femaleSource){
    RetainedRotatingWheelCalibrationResult result;
    result.artifactIdentity=artifactIdentity();
    result.orientationIdentity=QStringLiteral("30027b-wheel-face-down-bearing-axis-perpendicular-v1");
    const auto features=RetainedRotatingWheelSemantic::recognize(femaleSource);
    if(features.size()!=1||features.front().role!=FunctionalInterfaceRole::Female||
       !femaleSource.sourceModel||femaleSource.sourceModel->files.isEmpty()||
       femaleSource.sourceModel->files.front().relativePath.toLower()!=QStringLiteral("parts/30027b.dat")){
        result.diagnostic=QStringLiteral("The first fixture requires the certified 30027b notched wheel source.");
        return result;
    }
    result.regenerationPrototype=features.front();
    const auto& frame=features.front().frame;
    int boreVertices=0,notchVertices=0;
    for(const auto& triangle:femaleSource.mesh.triangles)for(const auto& vertex:{triangle.a,triangle.b,triangle.c}){
        const auto relative=subtract(converted(vertex),frame.origin);
        const auto transverse=subtract(relative,scaled(frame.axis,dot(relative,frame.axis)));
        const auto radius=length(transverse);
        if(std::abs(radius-1.6)<.01)++boreVertices;
        if(radius>1.8&&radius<2.01)++notchVertices;
    }
    if(boreVertices<24||notchVertices<8){
        result.diagnostic=QStringLiteral("Both the certified cylindrical bearing and notched retention entry are required.");
        return result;
    }
    for(int i=0;i<7;++i){
        const double correction=(i-3)*.05;
        auto candidate=femaleSource;
        if(correction!=0)for(auto& triangle:candidate.mesh.triangles)
            for(auto* vertex:{&triangle.a,&triangle.b,&triangle.c}){
                const auto p=converted(*vertex),relative=subtract(p,frame.origin);
                const double axial=dot(relative,frame.axis);
                const auto transverse=subtract(relative,scaled(frame.axis,axial));
                const double radius=length(transverse);
                if(radius<1.25||radius>=2.4)continue;
                // Move bearing and notched entry together. Fade to zero at
                // the untouched surrounding wheel web; all coincident source
                // vertices receive the same displacement.
                const double weight=radius<=2.0?1.0:(2.4-radius)/.4;
                *vertex=sourcePoint(add(p,scaled(transverse,correction*.5*weight/radius)));
            }
        const auto solidified=SourceSurfaceSolidifier::solidify(candidate);
        if(!solidified.successful||!validatePreparedMesh(solidified.analysis).ok()){
            result.diagnostic=QStringLiteral("Wheel candidate %1 failed source-faithful preparation: %2")
                .arg(i+1).arg(solidified.diagnostic);return result;
        }
        PrintMesh piece=solidified.mesh;
        const auto bounds=solidified.analysis.bounds;
        const double centerZ=(bounds.minimum.z+bounds.maximum.z)*.5;
        const double padTop=bounds.maximum.y;
        McutMeshBooleanService boolean;
        // Use the existing 1.04 mm by 0.60 mm relief dots on the solid
        // exterior rim, clear of the central bearing and its notched entry.
        for(int marker=0;marker<=i;++marker){
            const double angle=.15+2.0*3.14159265358979323846*marker/7.0;
            const auto dot=HingeCalibrationDotMarker::dot(3.4*std::cos(angle),
                centerZ+3.4*std::sin(angle),padTop);
            const auto joined=boolean.unite(piece,dot);
            if(!joined.ok()||!validatePreparedMesh(joined.resultAnalysis).ok()){
                result.diagnostic=QStringLiteral("Candidate %1 dot could not be joined.").arg(i+1);return result;
            }
            piece=joined.mesh;
        }
        double minimum=1e100;
        for(const auto& p:piece.vertices)minimum=std::min(minimum,p.y);
        for(auto& p:piece.vertices){const auto y=p.y;p.y=-p.z;p.z=y-minimum;}
        const auto analysis=analyzeSource(piece);
        if(!validatePreparedMesh(analysis).ok()||analysis.bounds.minimum.z<-.01){
            result.diagnostic=QStringLiteral("Candidate %1 is not a valid flat-printed wheel.").arg(i+1);return result;
        }
        result.candidateMeshes.push_back(std::move(piece));
        result.candidates.push_back({i+1,correction,3.2+correction});
    }
    result.ok=true;
    result.diagnostic=QStringLiteral("Seven source-faithful notched wheel bearings; the 3.20 mm contact bore and retention entry vary together while the exterior rim remains nominal.");
    return result;
}

FitCalibrationExperiment RetainedRotatingWheelCalibrationArtifact::observationTemplate(
    const RetainedRotatingWheelCalibrationResult& result){
    FitCalibrationExperiment e;
    e.artifactIdentity=result.artifactIdentity;
    e.featureFamily=QStringLiteral("RetainedRotatingWheel");
    e.featureRole=QStringLiteral("female");
    e.modeledOrientationIdentity=result.orientationIdentity;
    e.candidateSpacingMillimetres=.05;
    e.regenerationPrototype=result.regenerationPrototype;
    e.hasRegenerationPrototype=true;
    e.candidates=result.candidates;
    e.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
    e.process.orientationNotes=QStringLiteral("Print the seven dot-marked notched wheels flat, bearing axes perpendicular to the build plate, at 100% scale. Keep supports out of both sides of the bearing and notched entry. Repeatedly fit each wheel to the same genuine LEGO 4488 wheel holder; record insertion force, snap retention, free rotation, radial play, removal force, and wear. Four dots is the nominal 3.20 mm bearing and unmodified retention entry. The genuine pin is the fixed reference.");
    e.process.dimensionalCompensationNotes=QStringLiteral("Record actual slicer dimensional compensation settings before testing.");
    return e;
}
}

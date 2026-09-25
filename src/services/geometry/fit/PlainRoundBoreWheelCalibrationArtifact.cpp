#include "PlainRoundBoreWheelCalibrationArtifact.h"
#include "HingeCalibrationDotMarker.h"
#include "../print/PlainRoundBoreWheelSemantic.h"
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

QString PlainRoundBoreWheelCalibrationArtifact::artifactIdentity(){
    return QStringLiteral("plain-round-wheel-30027s01-blind-bore-perpendicular-coarse-v1");
}

PlainRoundBoreWheelCalibrationResult PlainRoundBoreWheelCalibrationArtifact::generate(
    const LDrawGeometry::LDrawLoadResult& femaleSource){
    const auto features=PlainRoundBoreWheelSemantic::recognize(femaleSource);
    if(features.size()!=1){PlainRoundBoreWheelCalibrationResult result;
        result.diagnostic=QStringLiteral("The certified plain wheel blind-bore source is unavailable.");return result;}
    PlainRoundBoreWheelCalibrationDefinition definition;
    definition.artifactIdentity=artifactIdentity();
    return generate(femaleSource,features.front(),definition);
}

PlainRoundBoreWheelCalibrationResult PlainRoundBoreWheelCalibrationArtifact::generate(
    const LDrawGeometry::LDrawLoadResult& femaleSource,const FunctionalFeature& prototype,
    const PlainRoundBoreWheelCalibrationDefinition& definition){
    PlainRoundBoreWheelCalibrationResult result;
    result.artifactIdentity=definition.artifactIdentity;
    result.parentArtifactIdentity=definition.parentArtifactIdentity;
    result.orientationIdentity=QStringLiteral("30027a-wheel-face-down-blind-bearing-axis-perpendicular-v1");
    const auto features=PlainRoundBoreWheelSemantic::recognize(femaleSource);
    if(features.size()!=1||!femaleSource.sourceModel||femaleSource.sourceModel->files.isEmpty()||
       femaleSource.sourceModel->files.front().relativePath.toLower()!=QStringLiteral("parts/30027a.dat")||
       prototype.family!=FunctionalInterfaceFamily::PlainRoundBoreWheel||
       prototype.role!=FunctionalInterfaceRole::Female||
       prototype.constructionRecipe!=features.front().constructionRecipe||
       prototype.evidenceContract!=features.front().evidenceContract||
       prototype.stableIdentity!=features.front().stableIdentity||
       std::abs(prototype.nominalDiameterMillimetres-features.front().nominalDiameterMillimetres)>1e-6||
       std::abs(prototype.nominalEngagementExtentMillimetres-features.front().nominalEngagementExtentMillimetres)>1e-6||
       definition.artifactIdentity.isEmpty()||definition.candidateCount<3||
       definition.candidateCount>9||definition.candidateCount%2==0||
       definition.candidateSpacingMillimetres<=0){
        result.diagnostic=QStringLiteral("The first fixture requires the certified 30027a blind round-bore wheel source.");
        return result;
    }
    result.regenerationPrototype=features.front();
    const auto& frame=features.front().frame;
    int bearingVertices=0,stopVertices=0;
    for(const auto& triangle:femaleSource.mesh.triangles)for(const auto& vertex:{triangle.a,triangle.b,triangle.c}){
        const auto relative=subtract(converted(vertex),frame.origin);
        const double axial=dot(relative,frame.axis);
        const double radius=length(subtract(relative,scaled(frame.axis,axial)));
        if(axial>=0&&axial<=4.0&&std::abs(radius-1.6)<.02)++bearingVertices;
        if(std::abs(axial-4.8)<.02&&radius<=1.65)++stopVertices;
    }
    if(bearingVertices<32||stopVertices<8){
        result.diagnostic=QStringLiteral("The certified continuous bearing or blind axial stop is missing.");
        return result;
    }
    for(int i=0;i<definition.candidateCount;++i){
        const double correction=definition.centerCorrectionMillimetres+
            (i-definition.candidateCount/2)*definition.candidateSpacingMillimetres;
        if(3.2+correction<=0){result.diagnostic=QStringLiteral("The blind-bore diameter must remain positive.");return result;}
        auto candidate=femaleSource;
        if(correction!=0)for(auto& triangle:candidate.mesh.triangles)
            for(auto* vertex:{&triangle.a,&triangle.b,&triangle.c}){
                const auto p=converted(*vertex),relative=subtract(p,frame.origin);
                const double axial=dot(relative,frame.axis);
                if(axial<-.02||axial>=4.8)continue;
                const auto transverse=subtract(relative,scaled(frame.axis,axial));
                const double radius=length(transverse);
                if(radius<1.25||radius>=2.4)continue;
                // The continuous blind bearing changes radially. The opening
                // rim follows it, while the closed stop and outer web remain
                // fixed; the end of the bore tapers back into that stop.
                const double radialWeight=radius<=2.0?1.0:(2.4-radius)/.4;
                const double stopWeight=axial<=4.0?1.0:std::clamp((4.8-axial)/.8,0.0,1.0);
                *vertex=sourcePoint(add(p,scaled(transverse,
                    correction*.5*radialWeight*stopWeight/radius)));
            }
        const auto solidified=SourceSurfaceSolidifier::solidify(candidate);
        if(!solidified.successful||!validatePreparedMesh(solidified.analysis).ok()){
            result.diagnostic=QStringLiteral("Plain-wheel candidate %1 failed source-faithful preparation: %2")
                .arg(i+1).arg(solidified.diagnostic);return result;
        }
        PrintMesh piece=solidified.mesh;
        const auto bounds=solidified.analysis.bounds;
        const double centerZ=(bounds.minimum.z+bounds.maximum.z)*.5;
        const double rimTop=bounds.maximum.y;
        McutMeshBooleanService boolean;
        for(int marker=0;marker<=i;++marker){
            const double angle=.15+2.0*3.14159265358979323846*marker/double(definition.candidateCount);
            const auto dot=HingeCalibrationDotMarker::dot(3.4*std::cos(angle),
                centerZ+3.4*std::sin(angle),rimTop);
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
            result.diagnostic=QStringLiteral("Candidate %1 is not a valid flat-printed blind-bore wheel.").arg(i+1);
            return result;
        }
        result.candidateMeshes.push_back(std::move(piece));
        result.candidates.push_back({i+1,correction,3.2+correction});
    }
    result.ok=true;
    result.diagnostic=QStringLiteral("%1 source-faithful plain blind-bore wheels; the continuous bearing opening varies while its axial stop and wheel exterior remain nominal.").arg(definition.candidateCount);
    return result;
}

FitCalibrationExperiment PlainRoundBoreWheelCalibrationArtifact::observationTemplate(
    const PlainRoundBoreWheelCalibrationResult& result){
    FitCalibrationExperiment e;
    e.artifactIdentity=result.artifactIdentity;
    e.parentArtifactIdentity=result.parentArtifactIdentity;
    e.featureFamily=QStringLiteral("PlainRoundBoreWheel");
    e.featureRole=QStringLiteral("female");
    e.modeledOrientationIdentity=result.orientationIdentity;
    e.candidateSpacingMillimetres=result.candidates.size()>1?
        result.candidates[1].diameterCorrectionMillimetres-result.candidates[0].diameterCorrectionMillimetres:.1;
    e.centerDiameterCorrectionMillimetres=result.candidates.isEmpty()?0.0:
        result.candidates[result.candidates.size()/2].diameterCorrectionMillimetres;
    e.regenerationPrototype=result.regenerationPrototype;
    e.hasRegenerationPrototype=true;
    e.candidates=result.candidates;
    e.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
    e.process.orientationNotes=QStringLiteral("Print each dot-marked 30027a-style wheel flat at 100% scale with its blind bearing opening facing up and axis perpendicular to the build plate. Keep supports out of the round bore. Repeatedly fit each candidate to the same genuine LEGO 4488 wheel-holder pin; record insertion force, free rotation, radial play, lip retention/slip, removal force, and wear. Dot counts identify candidate numbers; use the session dimensions rather than treating the middle candidate as nominal in an extension. Do not use the notched 30027b fixture or its calibration evidence.");
    e.process.dimensionalCompensationNotes=QStringLiteral("Record actual slicer dimensional compensation settings before testing.");
    return e;
}
}

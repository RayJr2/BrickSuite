#include "ClickHingeCalibrationArtifact.h"
#include "HingeCalibrationDotMarker.h"

#include "../print/ClickHingeSemantic.h"
#include "../print/McutMeshBooleanService.h"
#include "../print/PrintMeshAnalysis.h"
#include "../print/SourceSurfaceSolidifier.h"

#include <QHash>
#include <algorithm>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point converted(const QVector3D& p){return {double(p.x())*MmPerLdu,double(p.z())*MmPerLdu,-double(p.y())*MmPerLdu};}
QVector3D sourcePoint(Point p){return {float(p.x/MmPerLdu),float(-p.z/MmPerLdu),float(p.y/MmPerLdu)};}
Point add(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Point subtract(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point scaled(Point a,double s){return {a.x*s,a.y*s,a.z*s};}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double length(Point a){return std::sqrt(dot(a,a));}
Point origin(const std::array<double,12>& t){return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu};}
Point column(const std::array<double,12>& t,int c){return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu};}
QString key(Point p){return QStringLiteral("%1|%2|%3").arg(std::llround(p.x*100000.0))
    .arg(std::llround(p.y*100000.0)).arg(std::llround(p.z*100000.0));}
bool descendant(const LDrawGeometry::LDrawSourceModel& model,int child,int owner){
    while(child>=0&&child<model.references.size()&&child!=owner)child=model.references[child].parentId;
    return child==owner;
}
double smooth(double x){x=std::clamp(x,0.0,1.0);return x*x*(3.0-2.0*x);}
PrintMesh box(double x0,double x1,double y0,double y1,double z0,double z1){
    PrintMesh m;
    m.vertices={{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},
                {x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}};
    m.faces={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
             {1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
    return m;
}
} // namespace

QString ClickHingeCalibrationArtifact::artifactIdentity(){
    return QStringLiteral("click-hinge-clh1-arrestor-parallel-coarse-v1");
}

ClickHingeCalibrationResult ClickHingeCalibrationArtifact::generate(const LDrawGeometry::LDrawLoadResult& source){
    ClickHingeCalibrationResult result;
    result.artifactIdentity=artifactIdentity();
    result.orientationIdentity=QStringLiteral("30345-exterior-side-down-axis-parallel-v1");
    const auto features=ClickHingeSemantic::recognize(source);
    if(features.size()!=1||features.front().role!=FunctionalInterfaceRole::Male||!source.sourceModel){
        result.diagnostic=QStringLiteral("A certified clh1 single-finger source is required.");return result;
    }
    result.regenerationPrototype=features.front();
    const auto& model=*source.sourceModel;
    const int owner=features.front().provenance.front().referenceId;
    if(owner<0||owner>=model.references.size())return result;
    const auto& t=model.references[owner].accumulatedTransform;
    const auto base=origin(t);
    const auto x=scaled(column(t,0),1.0/length(column(t,0)));
    const auto y=scaled(column(t,1),1.0/length(column(t,1)));
    const auto outward=scaled(column(t,2),1.0/length(column(t,2)));
    QHash<QString,double> weights;
    int ownedFaces=0,contactVertices=0,oppositeContactVertices=0;
    for(const auto& surface:model.surfaces)if(descendant(model,surface.referenceId,owner)){
        if(!surface.certified||surface.triangleIndex<0||surface.triangleIndex>=source.mesh.triangles.size()){
            result.diagnostic=QStringLiteral("The certified clh1 source contact is incomplete.");return result;
        }
        ++ownedFaces;
        const auto& triangle=source.mesh.triangles[surface.triangleIndex];
        for(const auto& vertex:{triangle.a,triangle.b,triangle.c}){
            const auto p=converted(vertex),relative=subtract(p,base);
            const double axial=dot(relative,x)/MmPerLdu;
            const double across=dot(relative,y)/MmPerLdu;
            const double radial=dot(relative,outward)/MmPerLdu;
            // clh1's two source-owned arrestor contacts lie at axial +/-9 LDU,
            // across +/-3 LDU and radial -2.9..-4.535 LDU. Taper at their
            // roots; never move the bearing, finger shell or attachment box.
            const double weight=smooth((std::abs(axial)-3.8)/.9)*
                smooth((9.6-std::abs(axial))/.5)*
                smooth((3.7-std::abs(across))/.8)*
                smooth((radial+5.1)/.7)*smooth((-2.4-radial)/.5);
            if(weight<.01)continue;
            auto& stored=weights[key(p)];stored=std::max(stored,weight);
            if(weight>.7){if(axial<0)++contactVertices;else ++oppositeContactVertices;}
        }
    }
    if(ownedFaces<200||weights.size()<32||contactVertices<8||oppositeContactVertices<8){
        result.diagnostic=QStringLiteral("Both clh1 arrestor contact regions must be certified and distinct.");return result;
    }
    for(int i=0;i<7;++i){
        const double correction=(i-3)*.05;
        auto candidate=source;
        if(correction!=0)for(auto& triangle:candidate.mesh.triangles)
            for(auto* vertex:{&triangle.a,&triangle.b,&triangle.c}){
                const auto p=converted(*vertex);
                const auto weight=weights.constFind(key(p));
                if(weight!=weights.cend())*vertex=sourcePoint(add(p,scaled(outward,correction*(*weight))));
            }
        const auto solidified=SourceSurfaceSolidifier::solidify(candidate);
        if(!solidified.successful||!validatePreparedMesh(solidified.analysis).ok()){
            result.diagnostic=QStringLiteral("Click arrestor candidate %1 failed source-faithful preparation: %2")
                .arg(i+1).arg(solidified.diagnostic);return result;
        }
        PrintMesh piece=solidified.mesh;
        const auto bounds=solidified.analysis.bounds;
        // A small label pad overlaps only the non-fit exterior top of the
        // insert body; the arrestors and indexed mating surfaces stay clear.
        const double centerZ=(bounds.minimum.z+bounds.maximum.z)*.5;
        const double topY=bounds.maximum.y+.38;
        McutMeshBooleanService boolean;
        PrintMesh label=box(-1.55,1.55,bounds.maximum.y-.25,topY,centerZ-3.2,centerZ+3.2);
        for(const auto& dot:HingeCalibrationDotMarker::dots(i+1,0,centerZ,topY)){
            const auto joined=boolean.unite(label,dot);
            if(!joined.ok()||!validatePreparedMesh(joined.resultAnalysis).ok()){
                result.diagnostic=QStringLiteral("Candidate %1 exterior dot marker could not be joined.").arg(i+1);
                return result;
            }
            label=joined.mesh;
        }
        const auto marked=boolean.unite(piece,label);
        if(!marked.ok()||!validatePreparedMesh(marked.resultAnalysis).ok()){
            result.diagnostic=QStringLiteral("Candidate %1 exterior label could not be joined: %2")
                .arg(i+1).arg(QString::fromStdString(marked.message));return result;
        }
        piece=marked.mesh;
        // Place the insert body on its exterior side while the click axis
        // remains parallel to the build plate and contact features stay aloft.
        double minimum=1e100;
        for(const auto& p:piece.vertices)minimum=std::min(minimum,p.x);
        for(auto& p:piece.vertices){const double x0=p.x,z0=p.z;p.x=-z0;p.z=x0-minimum;}
        const auto analysis=analyzeSource(piece);
        if(!validatePreparedMesh(analysis).ok()||analysis.bounds.minimum.z<-.01){
            result.diagnostic=QStringLiteral("Candidate %1 is not a valid side-down printed solid.").arg(i+1);
            return result;
        }
        // The certified arrestor shoulder is at -4.535 LDU from a bearing
        // centered at -10 LDU: its nominal radial reach is 2.186 mm.
        result.candidates.push_back({i+1,0,0,correction,2.186+correction});
        result.candidateMeshes.push_back(std::move(piece));
    }
    result.ok=true;
    result.diagnostic=QStringLiteral("Seven source-faithful clh1 click-lock inserts; both arrestor contact regions vary together while the hinge bearing and attachment remain fixed.");
    return result;
}

FitCalibrationExperiment ClickHingeCalibrationArtifact::observationTemplate(
    const ClickHingeCalibrationResult& result){
    FitCalibrationExperiment experiment;
    experiment.artifactIdentity=result.artifactIdentity;
    experiment.featureFamily=QStringLiteral("ClickHinge");
    experiment.featureRole=QStringLiteral("male");
    experiment.modeledOrientationIdentity=result.orientationIdentity;
    experiment.correctionDimension=FitCorrectionDimension::Height;
    experiment.centerHeightCorrectionMillimetres=0;
    experiment.candidateSpacingMillimetres=.05;
    experiment.regenerationPrototype=result.regenerationPrototype;
    experiment.hasRegenerationPrototype=true;
    experiment.candidates=result.candidates;
    experiment.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
    experiment.process.orientationNotes=QStringLiteral("Print each dot-marked 30345 click-lock insert on its exterior long side at 100% scale with the rotation axis parallel to the bed. Keep supports off both arrestors and the bearing. Engage each with the same genuine paired-clh4 mating part, such as 30394. Repeatedly assess insertion, indexed rotation, holding torque, release force, play, and wear. Four dots (Candidate #4) retains the unmodified 2.186 mm source arrestor radial reach; the seven corrections are offsets relative to that reference.");
    experiment.process.dimensionalCompensationNotes=QStringLiteral("Record actual slicer dimensional compensation settings before testing.");
    return experiment;
}
}

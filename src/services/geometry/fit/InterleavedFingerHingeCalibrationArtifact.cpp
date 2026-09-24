#include "InterleavedFingerHingeCalibrationArtifact.h"
#include "HingeCalibrationDotMarker.h"

#include "../print/InterleavedFingerHingeSemantic.h"
#include "../print/McutMeshBooleanService.h"
#include "../print/PrintMeshAnalysis.h"
#include "../print/SourceSurfaceSolidifier.h"

#include <QHash>
#include <algorithm>
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
QString path(const LDrawGeometry::LDrawSourceModel& model,const LDrawGeometry::ReferenceRecord& ref) {
    return ref.fileId>=0&&ref.fileId<model.files.size()?model.files[ref.fileId].relativePath.toLower():QString{};
}
Point origin(const std::array<double,12>& t) { return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu}; }
Point column(const std::array<double,12>& t,int c) { return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu}; }
PrintMesh box(double x0,double x1,double y0,double y1,double z0,double z1) {
    PrintMesh m;
    m.vertices={{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},
                {x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}};
    m.faces={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
             {1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
    return m;
}
struct Movement { Point sum;int count=0; };
} // namespace

QString InterleavedFingerHingeCalibrationArtifact::artifactIdentity() {
    return QStringLiteral("interleaved-finger-hinge-contact-bump-parallel-coarse-v1");
}

InterleavedFingerHingeCalibrationResult InterleavedFingerHingeCalibrationArtifact::generate(
    const LDrawGeometry::LDrawLoadResult& source) {
    InterleavedFingerHingeCalibrationResult result;
    result.artifactIdentity=artifactIdentity();
    result.orientationIdentity=QStringLiteral("4275a-side-down-finger-axis-parallel-v1");
    if(!source.ok()||!source.sourceModel) {
        result.diagnostic=QStringLiteral("A certified three-finger LDraw source is required.");return result;
    }
    const auto features=InterleavedFingerHingeSemantic::recognize(source);
    if(features.size()!=1||features.front().role!=FunctionalInterfaceRole::Male) {
        result.diagnostic=QStringLiteral("The source must contain the certified h2 three-finger hinge.");return result;
    }
    result.regenerationPrototype=features.front();
    const auto& model=*source.sourceModel;
    const int owner=features.front().provenance.back().referenceId;
    QVector<int> bumps;
    for(const auto& ref:model.references)
        if(ref.parentId==owner&&path(model,ref)==QStringLiteral("p/bump5000.dat"))bumps.push_back(ref.id);
    if(bumps.size()!=2) {
        result.diagnostic=QStringLiteral("Both certified contact bumps are required.");return result;
    }
    QHash<QString,Movement> movements;
    int ownedFaces=0,movedTipVertices=0;
    for(const int bump:bumps) {
        const auto& ref=model.references[bump];
        const Point base=origin(ref.accumulatedTransform);
        const Point axial=column(ref.accumulatedTransform,1);
        const Point tip=scaled(axial,-1.0/length(axial));
        const double nominalHeight=.5*length(axial);
        if(std::abs(nominalHeight-.3)>.001) {
            result.diagnostic=QStringLiteral("Unexpected certified contact-bump height.");return result;
        }
        for(const auto& surface:model.surfaces) {
            if(!descendant(model,surface.referenceId,bump)||!surface.certified||
               surface.triangleIndex<0||surface.triangleIndex>=source.mesh.triangles.size())continue;
            ++ownedFaces;
            const auto& triangle=source.mesh.triangles[surface.triangleIndex];
            for(const auto& vertex:{triangle.a,triangle.b,triangle.c}) {
                const Point p=converted(vertex);
                const double h=dot(subtract(p,base),tip);
                if(h<-.001||h>nominalHeight+.001)continue;
                if(h>nominalHeight*.8)++movedTipVertices;
                auto& movement=movements[key(p)];
                movement.sum=add(movement.sum,scaled(tip,std::clamp(h/nominalHeight,0.0,1.0)));
                ++movement.count;
            }
        }
    }
    if(ownedFaces<96||movements.size()<48||movedTipVertices<16) {
        result.diagnostic=QStringLiteral("The certified bump surfaces are incomplete.");return result;
    }
    for(int i=0;i<7;++i) {
        const double correction=(i-3)*.05;
        auto candidate=source;
        if(correction!=0)for(auto& triangle:candidate.mesh.triangles)
            for(auto* vertex:{&triangle.a,&triangle.b,&triangle.c}) {
                const Point p=converted(*vertex);
                const auto movement=movements.constFind(key(p));
                if(movement==movements.cend())continue;
                *vertex=sourcePoint(add(p,scaled(movement->sum,correction/movement->count)));
            }
        const auto solidified=SourceSurfaceSolidifier::solidify(candidate);
        if(!solidified.successful||!validatePreparedMesh(solidified.analysis).ok()) {
            result.diagnostic=QStringLiteral("Candidate %1 failed source-surface preparation: %2")
                .arg(i+1).arg(solidified.diagnostic);return result;
        }
        PrintMesh prepared=solidified.mesh;
        // An exterior marker pad joins the back of the plate, away from both
        // certified contact bumps, interleaving fingers and stud attachments.
        const auto bounds=solidified.analysis.bounds;
        McutMeshBooleanService boolean;
        const double x=0.0;
        const double y=bounds.maximum.y+.40;
        PrintMesh label=box(x-1.55,x+1.55,bounds.maximum.y-.22,y,
            -4.8,1.6);
        for(const auto& dot:HingeCalibrationDotMarker::dots(i+1,x,-1.6,y)) {
            const auto joined=boolean.unite(label,dot);
            if(!joined.ok()||!validatePreparedMesh(joined.resultAnalysis).ok()) {
                result.diagnostic=QStringLiteral("Candidate %1 dots failed to join.").arg(i+1);return result;
            }
            label=joined.mesh;
        }
        const auto marked=boolean.unite(prepared,label);
        if(!marked.ok()||!validatePreparedMesh(marked.resultAnalysis).ok()) {
            result.diagnostic=QStringLiteral("Candidate %1 exterior marker failed: %2")
                .arg(i+1).arg(QString::fromStdString(marked.message));return result;
        }
        prepared=marked.mesh;
        // Stand the long exterior plate edge on the bed. The h2 axis stays
        // parallel to the bed while the contact geometry is elevated.
        double minimum=1e100;
        for(const auto& p:prepared.vertices)minimum=std::min(minimum,p.x);
        for(auto& p:prepared.vertices) {
            const double x0=p.x,z0=p.z;
            p.x=-z0;p.z=x0-minimum;
        }
        const auto analysis=analyzeSource(prepared);
        if(!validatePreparedMesh(analysis).ok()||analysis.bounds.minimum.z<-.01) {
            result.diagnostic=QStringLiteral("Candidate %1 is not a valid side-down solid.").arg(i+1);return result;
        }
        result.candidates.push_back({i+1,0,0,correction,.3+correction});
        result.candidateMeshes.push_back(std::move(prepared));
    }
    result.ok=true;
    result.diagnostic=QStringLiteral("Seven source-faithful three-finger hinge pieces; %1 certified contact-bump triangles govern protrusion; complementary finger geometry remains fixed.")
        .arg(ownedFaces);
    return result;
}

FitCalibrationExperiment InterleavedFingerHingeCalibrationArtifact::observationTemplate(
    const InterleavedFingerHingeCalibrationResult& result) {
    FitCalibrationExperiment e;
    e.artifactIdentity=result.artifactIdentity;
    e.featureFamily=QStringLiteral("InterleavedFingerHinge");
    e.featureRole=QStringLiteral("male");
    e.modeledOrientationIdentity=result.orientationIdentity;
    e.correctionDimension=FitCorrectionDimension::Height;
    e.centerHeightCorrectionMillimetres=0;
    e.candidateSpacingMillimetres=.05;
    e.regenerationPrototype=result.regenerationPrototype;
    e.hasRegenerationPrototype=true;
    e.candidates=result.candidates;
    e.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
    e.process.orientationNotes=QStringLiteral("Print each dot-marked 4275a three-finger hinge on its long exterior plate edge at 100% scale, with the finger axis parallel to the build plate and no support on the fingers or bumps. Mate each candidate with the same genuine LEGO 4276a two-finger hinge. Assess assembly, repeated rotation, axial alignment, play, binding, retention and wear. Four dots (Candidate #4) is the unmodified nominal 0.30 mm contact-bump protrusion; only the two source-owned contact bumps vary.");
    e.process.dimensionalCompensationNotes=QStringLiteral("Record actual slicer dimensional compensation settings before testing.");
    return e;
}
}

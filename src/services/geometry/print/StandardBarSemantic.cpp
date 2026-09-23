#include "StandardBarSemantic.h"
#include "PrintMeshAnalysis.h"

#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSet>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
constexpr double Tolerance=1e-4;
Point column(const std::array<double,12>& t,int c) { return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu}; }
Point origin(const std::array<double,12>& t) { return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu}; }
Point add(Point a,Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Point subtract(Point a,Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Point scaled(Point a,double s) { return {a.x*s,a.y*s,a.z*s}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Point a) { return std::sqrt(dot(a,a)); }
Point cross(Point a,Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z}; }
Point converted(const QVector3D& p) { return {double(p.x())*MmPerLdu,double(p.z())*MmPerLdu,-double(p.y())*MmPerLdu}; }
QString vertexKey(Point p) {
    return QStringLiteral("%1|%2|%3").arg(std::llround(p.x*10000.0))
        .arg(std::llround(p.y*10000.0)).arg(std::llround(p.z*10000.0));
}
bool close(Point a,Point b) { return length(subtract(a,b))<Tolerance; }
QString primitive(const LDrawGeometry::LDrawSourceModel& model,const LDrawGeometry::ReferenceRecord& ref) {
    return model.files[ref.fileId].relativePath.section('/',-1).toLower();
}
bool certifiedSurface(const LDrawGeometry::LDrawSourceModel& model,int reference,int minimum) {
    int count=0;
    for(const auto& surface:model.surfaces) if(surface.referenceId==reference) {
        if(!surface.certified) return false;
        ++count;
    }
    return count>=minimum;
}
} // namespace

QVector<FunctionalFeature> StandardBarSemantic::recognize(const LDrawGeometry::LDrawLoadResult& source) {
    if(!source.ok() || !source.sourceModel) return {};
    const auto& model=*source.sourceModel;
    if(model.references.isEmpty() || model.files.isEmpty() ||
       model.files[model.references.front().fileId].classification!=LDrawGeometry::SourceClassification::Part)
        return {};
    static const QRegularExpression description(QStringLiteral("^Bar\\s+\\d+(?:\\.\\d+)?L(?:\\s|$)"),QRegularExpression::CaseInsensitiveOption);
    if(!description.match(model.files[model.references.front().fileId].description).hasMatch()) return {};
    const LDrawGeometry::ReferenceRecord* cylinder=nullptr;
    QVector<const LDrawGeometry::ReferenceRecord*> discs;
    QVector<const LDrawGeometry::ReferenceRecord*> edges;
    for(int i=1;i<model.references.size();++i) {
        const auto& ref=model.references[i];
        if(ref.parentId!=model.references.front().id) continue;
        if(ref.fileId<0 || ref.fileId>=model.files.size()) return {};
        const QString name=primitive(model,ref);
        if(name==QStringLiteral("4-4cyli.dat")) { if(cylinder) return {}; cylinder=&ref; }
        else if(name==QStringLiteral("4-4disc.dat")) discs.push_back(&ref);
        else if(name==QStringLiteral("4-4edge.dat")) edges.push_back(&ref);
        else return {};
    }
    if(!cylinder || discs.size()!=2 || edges.size()!=2 || !certifiedSurface(model,cylinder->id,32) ||
       !certifiedSurface(model,discs[0]->id,16) || !certifiedSurface(model,discs[1]->id,16)) return {};
    const auto& t=cylinder->accumulatedTransform;
    const Point x=column(t,0),y=column(t,1),z=column(t,2);
    const double axial=length(y);
    if(std::abs(length(x)-1.6)>Tolerance || std::abs(length(z)-1.6)>Tolerance || axial<8.0 ||
       std::abs(dot(x,y))>Tolerance || std::abs(dot(z,y))>Tolerance || std::abs(dot(x,z))>Tolerance) return {};
    const Point start=origin(t),end=add(start,y);
    bool first=false,last=false;
    double firstNormal=0,lastNormal=0;
    for(const auto* disc:discs) {
        const auto& transform=disc->accumulatedTransform;
        if(std::abs(length(column(transform,0))-1.6)>Tolerance ||
           std::abs(length(column(transform,2))-1.6)>Tolerance) return {};
        if(close(origin(transform),start)) { first=true; firstNormal=dot(column(transform,1),y); }
        if(close(origin(transform),end)) { last=true; lastNormal=dot(column(transform,1),y); }
    }
    if(!first || !last || firstNormal*lastNormal>=0) return {};
    bool firstEdge=false,lastEdge=false;
    for(const auto* edge:edges) {
        const auto& transform=edge->accumulatedTransform;
        if(std::abs(length(column(transform,0))-1.6)>Tolerance ||
           std::abs(length(column(transform,2))-1.6)>Tolerance) return {};
        firstEdge|=close(origin(transform),start);
        lastEdge|=close(origin(transform),end);
    }
    if(!firstEdge || !lastEdge) return {};
    FunctionalFeature feature;
    feature.family=FunctionalInterfaceFamily::StandardBar;
    feature.role=FunctionalInterfaceRole::Male;
    feature.materialSide=FunctionalMaterialSide::MaterialInside;
    feature.eligibility=FunctionalEligibility::Eligible;
    feature.confidence=SemanticConfidence::HighConfidence;
    feature.operandAction=FunctionalOperandAction::Unite;
    feature.frame.origin=start;
    feature.frame.axis=scaled(y,1.0/axial);
    feature.frame.profileU=scaled(x,1.0/length(x));
    feature.frame.profileV=cross(feature.frame.axis,feature.frame.profileU);
    feature.frame.mirrored=cylinder->mirrored;
    feature.nominalRadiusMillimetres=1.6;
    feature.nominalDiameterMillimetres=3.2;
    feature.nominalAxialExtentMillimetres=axial;
    feature.nominalEngagementExtentMillimetres=axial;
    feature.radialProfile={{0,1.6},{axial,1.6}};
    feature.constructionRecipe=QStringLiteral("standard-bar-simple-cylinder-v1");
    feature.evidenceContract=QStringLiteral("official-ldraw-capped-standard-bar-v1");
    feature.provenance={{model.files[cylinder->fileId].relativePath,cylinder->id,cylinder->sourceLine,cylinder->inverted}};
    const QByteArray identity=QStringLiteral("%1|%2|%3").arg(model.files.front().relativePath)
        .arg(cylinder->sourceLine).arg(axial,0,'g',17).toUtf8();
    feature.stableIdentity=QStringLiteral("standard-bar:")+QString::fromLatin1(
        QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
    feature.governingOperandIdentity=feature.stableIdentity+QStringLiteral(":body-cylinder");
    return {feature};
}
bool StandardBarSemantic::adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
    const PrintMesh& nominal,const FunctionalFeature& bar,double correction,
    PrintMesh* adjusted,QString* diagnostic) {
    if(!adjusted || !source.ok() || !source.sourceModel || nominal.faces.empty() ||
       bar.family!=FunctionalInterfaceFamily::StandardBar ||
       bar.constructionRecipe!=QStringLiteral("standard-bar-simple-cylinder-v1") ||
       bar.evidenceContract!=QStringLiteral("official-ldraw-capped-standard-bar-v1") ||
       bar.provenance.size()!=1 || !std::isfinite(correction) || std::abs(correction)>0.5 ||
       bar.nominalRadiusMillimetres+correction*.5<=0) {
        if(diagnostic) *diagnostic=QStringLiteral("The Standard Bar correction or source contract is invalid.");
        return false;
    }
    const auto& model=*source.sourceModel;
    const int owner=bar.provenance.front().referenceId;
    if(owner<0 || owner>=model.references.size() ||
       primitive(model,model.references[owner])!=QStringLiteral("4-4cyli.dat")) {
        if(diagnostic) *diagnostic=QStringLiteral("The certified Standard Bar cylindrical owner is missing.");
        return false;
    }
    QSet<QString> owned;
    int certifiedTriangles=0;
    for(const auto& surface:model.surfaces) {
        if(surface.referenceId!=owner) continue;
        if(!surface.certified || surface.triangleIndex<0 ||
           surface.triangleIndex>=source.mesh.triangles.size()) {
            if(diagnostic) *diagnostic=QStringLiteral("The Standard Bar cylindrical source surface is not certified.");
            return false;
        }
        const auto& triangle=source.mesh.triangles[surface.triangleIndex];
        for(const auto& vertex:{triangle.a,triangle.b,triangle.c})
            owned.insert(vertexKey(converted(vertex)));
        ++certifiedTriangles;
    }
    if(certifiedTriangles<32 || owned.size()<32) {
        if(diagnostic) *diagnostic=QStringLiteral("The certified Standard Bar cylindrical source surface is incomplete.");
        return false;
    }
    // A Verified zero correction is still an applied profile result, but must not
    // perturb the nominal PreparedMesh's vertices, topology or identity inputs.
    if(correction==0.0) {
        *adjusted=nominal;
        if(diagnostic) *diagnostic=QStringLiteral("Verified zero Standard Bar correction retained the nominal PreparedMesh exactly.");
        return true;
    }
    PrintMesh result=nominal;
    int moved=0;
    for(auto& point:result.vertices) {
        if(!owned.contains(vertexKey(point))) continue;
        const auto relative=subtract(point,bar.frame.origin);
        const double axial=dot(relative,bar.frame.axis);
        const auto radial=subtract(relative,scaled(bar.frame.axis,axial));
        const double radius=length(radial);
        if(axial<-Tolerance || axial>bar.nominalAxialExtentMillimetres+Tolerance ||
           std::abs(radius-bar.nominalRadiusMillimetres)>Tolerance) continue;
        point=add(point,scaled(radial,correction/(2.0*radius)));
        ++moved;
    }
    const auto analysis=analyzeSource(result);
    if(moved<32 || !validatePreparedMesh(analysis).ok()) {
        if(diagnostic) *diagnostic=QStringLiteral("The source-owned Standard Bar diameter correction failed topology validation.");
        return false;
    }
    *adjusted=std::move(result);
    if(diagnostic) *diagnostic=QStringLiteral("Certified Standard Bar cylindrical wall adjusted; bar length and end planes retained.");
    return true;
}
} // namespace PrintGeometry

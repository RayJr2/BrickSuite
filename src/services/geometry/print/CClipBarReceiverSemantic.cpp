#include "CClipBarReceiverSemantic.h"

#include <QCryptographicHash>
#include "PrintMeshAnalysis.h"
#include <cmath>
#include <limits>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point column(const std::array<double,12>& t,int c) { return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu}; }
Point origin(const std::array<double,12>& t) { return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Point a) { return std::sqrt(dot(a,a)); }
Point scaled(Point a,double s) { return {a.x*s,a.y*s,a.z*s}; }
Point add(Point a,Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Point subtract(Point a,Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Point cross(Point a,Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z}; }
Point converted(const QVector3D& p) { return {p.x()*.4,p.z()*.4,-p.y()*.4}; }
double distanceSquared(Point a,Point b) { const auto d=subtract(a,b); return dot(d,d); }
Point closest(Point p,Point a,Point b,Point c) {
    const auto ab=subtract(b,a),ac=subtract(c,a),ap=subtract(p,a);
    const double d1=dot(ab,ap),d2=dot(ac,ap);
    if(d1<=0&&d2<=0)return a;
    const auto bp=subtract(p,b);const double d3=dot(ab,bp),d4=dot(ac,bp);
    if(d3>=0&&d4<=d3)return b;
    const double vc=d1*d4-d3*d2;
    if(vc<=0&&d1>=0&&d3<=0)return add(a,scaled(ab,d1/(d1-d3)));
    const auto cp=subtract(p,c);const double d5=dot(ab,cp),d6=dot(ac,cp);
    if(d6>=0&&d5<=d6)return c;
    const double vb=d5*d2-d1*d6;
    if(vb<=0&&d2>=0&&d6<=0)return add(a,scaled(ac,d2/(d2-d6)));
    const double va=d3*d6-d5*d4;
    if(va<=0&&d4-d3>=0&&d5-d6>=0){const auto bc=subtract(c,b);return add(b,scaled(bc,(d4-d3)/(d4-d3+d5-d6)));}
    const double inv=1.0/(va+vb+vc);return add(a,add(scaled(ab,vb*inv),scaled(ac,vc*inv)));
}
QString leaf(const LDrawGeometry::LDrawSourceModel& model,const LDrawGeometry::ReferenceRecord& ref) {
    return model.files[ref.fileId].relativePath.section('/',-1).toLower();
}
bool certifiedDescendants(const LDrawGeometry::LDrawSourceModel& model,int ancestor,int* count) {
    *count=0;
    for(const auto& surface:model.surfaces) {
        int ref=surface.referenceId;
        while(ref>=0 && ref<model.references.size() && ref!=ancestor)
            ref=model.references[ref].parentId;
        if(ref!=ancestor) continue;
        if(!surface.certified) return false;
        ++*count;
    }
    return true;
}
} // namespace

QVector<FunctionalFeature> CClipBarReceiverSemantic::recognize(const LDrawGeometry::LDrawLoadResult& source) {
    if(!source.ok() || !source.sourceModel) return {};
    const auto& model=*source.sourceModel;
    if(model.references.isEmpty() || model.files.isEmpty() ||
       model.files[model.references.front().fileId].classification!=LDrawGeometry::SourceClassification::Part)
        return {};
    QVector<FunctionalFeature> result;
    for(const auto& clip:model.references) {
        if(clip.fileId<0 || clip.fileId>=model.files.size() ||
           model.files[clip.fileId].relativePath.toLower()!=QStringLiteral("p/clip6.dat") ||
           clip.parentId!=model.references.front().id) continue;
        const LDrawGeometry::ReferenceRecord* inner=nullptr;
        for(const auto& child:model.references) {
            if(child.parentId!=clip.id || child.fileId<0 || child.fileId>=model.files.size()) continue;
            if(leaf(model,child)==QStringLiteral("5-8cyli.dat")) {
                if(inner) { inner=nullptr; break; }
                inner=&child;
            }
        }
        if(!inner) continue;
        const auto& t=inner->accumulatedTransform;
        const Point x=column(t,0),y=column(t,1),z=column(t,2);
        if(std::abs(length(x)-1.6)>1e-3 || std::abs(length(z)-1.6)>1e-3 ||
           std::abs(length(y)-3.2)>1e-3 || std::abs(dot(x,y))>1e-3 ||
           std::abs(dot(z,y))>1e-3 || std::abs(dot(x,z))>1e-3) continue;
        int innerTriangles=0,clipTriangles=0;
        if(!certifiedDescendants(model,inner->id,&innerTriangles) || innerTriangles<20 ||
           !certifiedDescendants(model,clip.id,&clipTriangles) || clipTriangles<80) continue;
        FunctionalFeature feature;
        feature.family=FunctionalInterfaceFamily::CClipBarReceiver;
        feature.role=FunctionalInterfaceRole::Female;
        feature.materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;
        feature.eligibility=FunctionalEligibility::Eligible;
        feature.confidence=SemanticConfidence::HighConfidence;
        feature.frame={origin(t),scaled(y,1.0/length(y)),scaled(x,1.0/length(x)),{},clip.mirrored};
        feature.frame.profileV=cross(feature.frame.axis,feature.frame.profileU);
        feature.nominalRadiusMillimetres=1.6;
        feature.nominalDiameterMillimetres=3.2;
        feature.nominalAxialExtentMillimetres=3.2;
        feature.nominalEngagementExtentMillimetres=3.2;
        feature.operandAction=FunctionalOperandAction::Subtract;
        feature.constructionRecipe=QStringLiteral("c-clip-compliant-receiver-v1");
        feature.evidenceContract=QStringLiteral("official-ldraw-clip6-bar-receiver-v1");
        feature.radialProfile={{0,1.6},{3.2,1.6}};
        feature.provenance={{model.files[clip.fileId].relativePath,clip.id,clip.sourceLine,clip.inverted},
                            {model.files[inner->fileId].relativePath,inner->id,inner->sourceLine,inner->inverted}};
        const QByteArray identity=QStringLiteral("%1|%2|%3").arg(model.files.front().relativePath)
            .arg(clip.sourceLine).arg(inner->sourceLine).toUtf8();
        feature.stableIdentity=QStringLiteral("c-clip-bar-receiver:")+
            QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
        feature.governingOperandIdentity=feature.stableIdentity+QStringLiteral(":inner-arc-and-throat");
        result.push_back(feature);
    }
    return result;
}

bool CClipBarReceiverSemantic::adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
    const PrintMesh& nominal,const FunctionalFeature& clip,double correction,
    PrintMesh* adjusted,QString* diagnostic) {
    if(!adjusted||!source.ok()||!source.sourceModel||nominal.faces.empty()||
       clip.family!=FunctionalInterfaceFamily::CClipBarReceiver||
       clip.constructionRecipe!=QStringLiteral("c-clip-compliant-receiver-v1")||
       clip.evidenceContract!=QStringLiteral("official-ldraw-clip6-bar-receiver-v1")||
       clip.provenance.size()!=2||!std::isfinite(correction)||std::abs(correction)>0.5||
       clip.nominalRadiusMillimetres+correction*.5<=0) {
        if(diagnostic)*diagnostic=QStringLiteral("The C-Clip correction or certified source contract is invalid.");
        return false;
    }
    const auto& model=*source.sourceModel;
    const int owner=clip.provenance.front().referenceId;
    if(owner<0||owner>=model.references.size()||
       model.files[model.references[owner].fileId].relativePath.toLower()!=QStringLiteral("p/clip6.dat")) {
        if(diagnostic)*diagnostic=QStringLiteral("The certified clip6 source owner is missing.");
        return false;
    }
    // Zero is a real Verified result. Preserve every nominal vertex and face bit-for-bit.
    if(correction==0.0) {
        *adjusted=nominal;
        if(diagnostic)*diagnostic=QStringLiteral("Verified zero C-Clip correction retained the nominal PreparedMesh exactly.");
        return true;
    }
    struct Surface {Point a,b,c;bool owned=false;};
    QVector<Surface> surfaces;
    surfaces.reserve(model.surfaces.size());
    for(const auto& surface:model.surfaces) {
        if(!surface.certified||surface.triangleIndex<0||surface.triangleIndex>=source.mesh.triangles.size()) {
            if(diagnostic)*diagnostic=QStringLiteral("The C-Clip source surface provenance is incomplete.");
            return false;
        }
        int ref=surface.referenceId;
        while(ref>=0&&ref<model.references.size()&&ref!=owner)ref=model.references[ref].parentId;
        const auto& t=source.mesh.triangles[surface.triangleIndex];
        surfaces.push_back({converted(t.a),converted(t.b),converted(t.c),ref==owner});
    }
    PrintMesh result=nominal;
    int moved=0;
    const double inner=clip.nominalRadiusMillimetres;
    // The certified inner arc and its adjacent throat lips control the snap
    // clearance. Fade to zero before the 2.7 mm outer arm envelope; never
    // move a vertex whose nearest authoritative surface is not clip-owned.
    constexpr double throatLimit=2.05;
    constexpr double ownershipTolerance=0.12;
    for(auto& point:result.vertices) {
        const auto relative=subtract(point,clip.frame.origin);
        const double axial=dot(relative,clip.frame.axis);
        const auto radial=subtract(relative,scaled(clip.frame.axis,axial));
        const double radius=length(radial);
        if(axial<-0.05||axial>clip.nominalAxialExtentMillimetres+0.05||
           radius<1.35||radius>=throatLimit)continue;
        double ownedDistance=std::numeric_limits<double>::max();
        double otherDistance=std::numeric_limits<double>::max();
        for(const auto& surface:surfaces) {
            const double d=distanceSquared(point,closest(point,surface.a,surface.b,surface.c));
            auto& nearest=surface.owned?ownedDistance:otherDistance;
            nearest=std::min(nearest,d);
        }
        if(ownedDistance>ownershipTolerance*ownershipTolerance||
           ownedDistance>otherDistance+1e-10)continue;
        const double weight=radius<=inner?1.0:(throatLimit-radius)/(throatLimit-inner);
        point=add(point,scaled(radial,correction*weight/(2.0*radius)));
        ++moved;
    }
    const auto analysis=analyzeSource(result);
    if(moved<24||!validatePreparedMesh(analysis).ok()) {
        if(diagnostic)*diagnostic=QStringLiteral("The source-owned C-Clip contact/throat correction failed strict topology validation.");
        return false;
    }
    *adjusted=std::move(result);
    if(diagnostic)*diagnostic=QStringLiteral("Certified clip6 inner contact arc and adjoining throat adjusted; outer arms and unrelated source surfaces retained (%1 vertices moved).").arg(moved);
    return true;
}
} // namespace PrintGeometry

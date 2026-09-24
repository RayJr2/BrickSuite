#include "RetainedRotatingWheelSemantic.h"
#include "PrintMeshAnalysis.h"

#include <QCryptographicHash>
#include <algorithm>
#include <cmath>
#include <limits>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point column(const std::array<double,12>& t,int c){return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu};}
Point origin(const std::array<double,12>& t){return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu};}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double length(Point a){return std::sqrt(dot(a,a));}
Point scaled(Point p,double s){return {p.x*s,p.y*s,p.z*s};}
Point add(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Point subtract(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point converted(const QVector3D& p){return {double(p.x())*MmPerLdu,double(p.z())*MmPerLdu,-double(p.y())*MmPerLdu};}
Point closest(Point p,Point a,Point b,Point c){
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
Point cross(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
QString path(const LDrawGeometry::LDrawSourceModel& model,const LDrawGeometry::ReferenceRecord& ref){
    return ref.fileId>=0&&ref.fileId<model.files.size()?model.files[ref.fileId].relativePath.toLower():QString{};
}
bool descendant(const LDrawGeometry::LDrawSourceModel& model,int child,int owner){
    while(child>=0&&child<model.references.size()&&child!=owner)child=model.references[child].parentId;
    return child==owner;
}
int certifiedCount(const LDrawGeometry::LDrawSourceModel& model,int owner){
    int count=0;
    for(const auto& surface:model.surfaces)if(descendant(model,surface.referenceId,owner)){
        if(!surface.certified)return -1;
        ++count;
    }
    return count;
}
bool certifiedScale(const std::array<double,12>& t,double axialLength){
    const auto x=column(t,0),y=column(t,1),z=column(t,2);
    return std::abs(length(x)-MmPerLdu)<1e-4&&std::abs(length(y)-axialLength)<1e-4&&
        std::abs(length(z)-MmPerLdu)<1e-4&&std::abs(dot(x,y))<1e-5&&
        std::abs(dot(x,z))<1e-5&&std::abs(dot(y,z))<1e-5;
}
FunctionalFeature feature(const LDrawGeometry::LDrawSourceModel& model,
    const LDrawGeometry::ReferenceRecord& owner,FunctionalInterfaceRole role){
    FunctionalFeature f;
    f.family=FunctionalInterfaceFamily::RetainedRotatingWheel;
    f.role=role;
    f.materialSide=role==FunctionalInterfaceRole::Male?FunctionalMaterialSide::MaterialInside:
        FunctionalMaterialSide::EmptyInsideMaterialOutside;
    f.operandAction=role==FunctionalInterfaceRole::Male?FunctionalOperandAction::Unite:
        FunctionalOperandAction::Subtract;
    f.eligibility=FunctionalEligibility::Eligible;
    f.confidence=SemanticConfidence::HighConfidence;
    const auto& t=owner.accumulatedTransform;
    const auto axis=scaled(column(t,1),1.0/length(column(t,1)));
    const auto u=scaled(column(t,0),1.0/MmPerLdu);
    f.frame={origin(t),axis,u,cross(axis,u),owner.mirrored};
    f.nominalRadiusMillimetres=1.6;
    f.nominalDiameterMillimetres=3.2;
    f.nominalAxialExtentMillimetres=role==FunctionalInterfaceRole::Male?4.8:3.2;
    f.nominalEngagementExtentMillimetres=f.nominalAxialExtentMillimetres;
    f.radialProfile={{0,1.6},{f.nominalAxialExtentMillimetres,1.6}};
    f.constructionRecipe=role==FunctionalInterfaceRole::Male?
        QStringLiteral("retained-wheel-wpin2a-slotted-pin-v1"):
        QStringLiteral("retained-wheel-wpinhol2-notched-bearing-v1");
    f.evidenceContract=QStringLiteral("official-ldraw-wpin2a-wpinhol2-retained-rotation-v1");
    f.provenance={{model.files[owner.fileId].relativePath,owner.id,owner.sourceLine,owner.inverted}};
    const auto bytes=QStringLiteral("%1|%2|%3").arg(model.files.front().relativePath)
        .arg(owner.sourceLine).arg(role==FunctionalInterfaceRole::Male?"pin":"bearing").toUtf8();
    f.stableIdentity=QStringLiteral("retained-wheel:")+
        QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
    f.governingOperandIdentity=f.stableIdentity+QStringLiteral(":contact-and-retention");
    return f;
}
} // namespace

QVector<FunctionalFeature> RetainedRotatingWheelSemantic::recognize(const LDrawGeometry::LDrawLoadResult& source){
    if(!source.ok()||!source.sourceModel)return {};
    const auto& model=*source.sourceModel;
    if(model.references.isEmpty()||model.files.isEmpty()||
       model.files[model.references.front().fileId].classification!=LDrawGeometry::SourceClassification::Part)return {};
    QVector<FunctionalFeature> result;
    for(const auto& ref:model.references){
        const auto p=path(model,ref);
        if(p==QStringLiteral("p/wpin2a.dat")){
            int core=0;
            for(const auto& child:model.references)if(child.parentId==ref.id&&
                path(model,child)==QStringLiteral("p/wpin.dat")&&certifiedCount(model,child.id)>=32)++core;
            if(core==1&&certifiedScale(ref.accumulatedTransform,.4)&&certifiedCount(model,ref.id)>=80)
                result.push_back(feature(model,ref,FunctionalInterfaceRole::Male));
        }else if(p==QStringLiteral("p/wpinhol2.dat")){
            if(certifiedScale(ref.accumulatedTransform,3.2)&&certifiedCount(model,ref.id)>=32)
                result.push_back(feature(model,ref,FunctionalInterfaceRole::Female));
        }
    }
    return result;
}

bool RetainedRotatingWheelSemantic::adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
    const PrintMesh& nominal,const FunctionalFeature& bearing,double correction,
    PrintMesh* adjusted,QString* diagnostic){
    if(!adjusted||!source.ok()||!source.sourceModel||nominal.faces.empty()||
       bearing.family!=FunctionalInterfaceFamily::RetainedRotatingWheel||
       bearing.role!=FunctionalInterfaceRole::Female||
       bearing.constructionRecipe!=QStringLiteral("retained-wheel-wpinhol2-notched-bearing-v1")||
       bearing.evidenceContract!=QStringLiteral("official-ldraw-wpin2a-wpinhol2-retained-rotation-v1")||
       bearing.provenance.size()!=1||!std::isfinite(correction)||std::abs(correction)>.35){
        if(diagnostic)*diagnostic=QStringLiteral("Invalid certified retained-wheel bearing correction.");
        return false;
    }
    const auto& model=*source.sourceModel;
    const int owner=bearing.provenance.front().referenceId;
    if(owner<0||owner>=model.references.size()||
       path(model,model.references[owner])!=QStringLiteral("p/wpinhol2.dat")){
        if(diagnostic)*diagnostic=QStringLiteral("Certified notched bearing owner is missing.");
        return false;
    }
    if(correction==0.0){*adjusted=nominal;return true;}
    struct Surface{Point a,b,c;bool owned;};
    QVector<Surface> surfaces;surfaces.reserve(model.surfaces.size());
    for(const auto& surface:model.surfaces){
        if(!surface.certified||surface.triangleIndex<0||surface.triangleIndex>=source.mesh.triangles.size())return false;
        const auto& t=source.mesh.triangles[surface.triangleIndex];
        surfaces.push_back({converted(t.a),converted(t.b),converted(t.c),
            descendant(model,surface.referenceId,owner)});
    }
    *adjusted=nominal;
    int movedBearing=0,movedEntry=0;
    for(auto& point:adjusted->vertices){
        const auto relative=subtract(point,bearing.frame.origin);
        const double axial=dot(relative,bearing.frame.axis);
        const auto radial=subtract(relative,scaled(bearing.frame.axis,axial));
        const double radius=length(radial);
        if(axial<-.18||axial>3.38||radius<1.25||radius>=2.4)continue;
        double ownedDistance=std::numeric_limits<double>::max(),otherDistance=ownedDistance;
        for(const auto& surface:surfaces){
            const double d=length(subtract(point,closest(point,surface.a,surface.b,surface.c)));
            auto& nearest=surface.owned?ownedDistance:otherDistance;
            nearest=std::min(nearest,d);
        }
        if(ownedDistance>.18||ownedDistance>otherDistance+1e-10)continue;
        const double weight=radius<=2.0?1.0:(2.4-radius)/.4;
        point=add(point,scaled(radial,correction*.5*weight/radius));
        if(radius<=1.8)++movedBearing;else ++movedEntry;
    }
    if(movedBearing<16||movedEntry<8||!validatePreparedMesh(analyzeSource(*adjusted)).ok()){
        if(diagnostic)*diagnostic=QStringLiteral("Retained-wheel bearing/entry correction failed strict topology validation (%1 bearing, %2 entry vertices).")
            .arg(movedBearing).arg(movedEntry);
        return false;
    }
    if(diagnostic)*diagnostic=QStringLiteral("Certified bearing and notched entry adjusted together (%1 bearing, %2 entry vertices); wheel exterior retained.")
        .arg(movedBearing).arg(movedEntry);
    return true;
}
}

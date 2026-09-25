#include "PlainRoundBoreWheelSemantic.h"
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
Point cross(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Point subtract(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point add(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
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
bool cylinder(const LDrawGeometry::ReferenceRecord& ref,double axial){
    const auto x=column(ref.accumulatedTransform,0),y=column(ref.accumulatedTransform,1),
        z=column(ref.accumulatedTransform,2);
    return std::abs(length(x)-1.6)<1e-4&&std::abs(length(y)-axial)<1e-4&&
        std::abs(length(z)-1.6)<1e-4&&std::abs(dot(x,y))<1e-5&&
        std::abs(dot(x,z))<1e-5&&std::abs(dot(y,z))<1e-5;
}
bool subpart(const QString& p,const QString& leaf){
    return p==QStringLiteral("s/")+leaf||p==QStringLiteral("parts/s/")+leaf;
}
}

QVector<FunctionalFeature> PlainRoundBoreWheelSemantic::recognize(
    const LDrawGeometry::LDrawLoadResult& source){
    if(!source.ok()||!source.sourceModel)return {};
    const auto& model=*source.sourceModel;
    if(model.references.isEmpty()||model.files.isEmpty()||
       model.files[model.references.front().fileId].classification!=LDrawGeometry::SourceClassification::Part)return {};
    // This source contract requires the wheel's blind, round bore, its
    // continuous rear segment and closed stop, and its characteristic wheel
    // web/slot subparts. Diameter alone is intentionally insufficient.
    int front=-1,rear=-1,sidePieces=0;
    for(const auto& ref:model.references){
        const auto p=path(model,ref);
        if(p==QStringLiteral("p/wpinhole.dat")||p==QStringLiteral("p/wpinhol2.dat"))return {};
        if(ref.parentId==model.references.front().id){
            if(p==QStringLiteral("p/4-4cyli.dat")&&cylinder(ref,3.2)&&certifiedCount(model,ref.id)>=24){
                if(front>=0)return {};
                front=ref.id;
            }else if(subpart(p,QStringLiteral("30027s01.dat"))){
                if(rear>=0)return {};
                rear=ref.id;
            }else if(subpart(p,QStringLiteral("30027s02.dat")))++sidePieces;
        }
    }
    if(front<0||rear<0||sidePieces!=2||certifiedCount(model,rear)<80)return {};
    const auto& bore=model.references[front];
    const auto axis=scaled(column(bore.accumulatedTransform,1),1.0/3.2);
    int rearBore=0,stop=0;
    for(const auto& ref:model.references)if(descendant(model,ref.id,rear)){
        const auto p=path(model,ref);
        const double axial=dot(subtract(origin(ref.accumulatedTransform),origin(bore.accumulatedTransform)),axis);
        if(p==QStringLiteral("p/4-4cyli.dat")&&cylinder(ref,.8)&&
           std::abs(axial-3.2)<1e-4&&certifiedCount(model,ref.id)>=24)++rearBore;
        if(p==QStringLiteral("p/4-4disc.dat")&&std::abs(axial-4.8)<1e-4&&
           certifiedCount(model,ref.id)>=8)++stop;
    }
    if(rearBore!=1||stop!=1)return {};
    FunctionalFeature f;
    f.family=FunctionalInterfaceFamily::PlainRoundBoreWheel;
    f.role=FunctionalInterfaceRole::Female;
    f.materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;
    f.operandAction=FunctionalOperandAction::Subtract;
    f.eligibility=FunctionalEligibility::Eligible;
    f.confidence=SemanticConfidence::HighConfidence;
    const auto u=scaled(column(bore.accumulatedTransform,0),1.0/1.6);
    f.frame={origin(bore.accumulatedTransform),axis,u,cross(axis,u),bore.mirrored};
    f.nominalRadiusMillimetres=1.6;
    f.nominalDiameterMillimetres=3.2;
    f.nominalAxialExtentMillimetres=4.8;
    f.nominalEngagementExtentMillimetres=4.8;
    f.radialProfile={{0,1.6},{4.8,1.6}};
    f.constructionRecipe=QStringLiteral("plain-wheel-30027s01-blind-round-bore-v1");
    f.evidenceContract=QStringLiteral("official-ldraw-30027s01-plain-wheel-blind-bore-v1");
    f.provenance={{model.files[bore.fileId].relativePath,bore.id,bore.sourceLine,bore.inverted},
                  {model.files[model.references[rear].fileId].relativePath,rear,
                   model.references[rear].sourceLine,model.references[rear].inverted}};
    const auto bytes=QStringLiteral("%1|%2|plain-wheel-blind-bore")
        .arg(model.files.front().relativePath).arg(bore.sourceLine).toUtf8();
    f.stableIdentity=QStringLiteral("plain-wheel:")+
        QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
    f.governingOperandIdentity=f.stableIdentity+QStringLiteral(":bearing-and-stop");
    return {f};
}

bool PlainRoundBoreWheelSemantic::adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
    const PrintMesh& nominal,const FunctionalFeature& bearing,double correction,
    PrintMesh* adjusted,QString* diagnostic){
    if(!adjusted||!source.ok()||!source.sourceModel||nominal.faces.empty()||
       bearing.family!=FunctionalInterfaceFamily::PlainRoundBoreWheel||
       bearing.role!=FunctionalInterfaceRole::Female||
       bearing.constructionRecipe!=QStringLiteral("plain-wheel-30027s01-blind-round-bore-v1")||
       bearing.evidenceContract!=QStringLiteral("official-ldraw-30027s01-plain-wheel-blind-bore-v1")||
       bearing.provenance.size()!=2||!std::isfinite(correction)||
       correction<-.5||correction>.8){
        if(diagnostic)*diagnostic=QStringLiteral("Invalid certified plain-wheel blind-bore correction.");
        return false;
    }
    const auto& model=*source.sourceModel;
    const int front=bearing.provenance[0].referenceId,rear=bearing.provenance[1].referenceId;
    if(front<0||front>=model.references.size()||rear<0||rear>=model.references.size()||
       path(model,model.references[front])!=QStringLiteral("p/4-4cyli.dat")||
       !subpart(path(model,model.references[rear]),QStringLiteral("30027s01.dat"))){
        if(diagnostic)*diagnostic=QStringLiteral("Certified continuous blind-bore owners are missing.");
        return false;
    }
    if(correction==0.0){*adjusted=nominal;return true;}
    struct Surface{Point a,b,c;bool owned;};
    QVector<Surface> surfaces;surfaces.reserve(model.surfaces.size());
    for(const auto& surface:model.surfaces){
        if(!surface.certified||surface.triangleIndex<0||surface.triangleIndex>=source.mesh.triangles.size())return false;
        const auto& t=source.mesh.triangles[surface.triangleIndex];
        surfaces.push_back({converted(t.a),converted(t.b),converted(t.c),
            descendant(model,surface.referenceId,front)||descendant(model,surface.referenceId,rear)});
    }
    *adjusted=nominal;
    int moved=0;
    for(auto& point:adjusted->vertices){
        const auto relative=subtract(point,bearing.frame.origin);
        const double axial=dot(relative,bearing.frame.axis);
        const auto radial=subtract(relative,scaled(bearing.frame.axis,axial));
        const double radius=length(radial);
        if(axial<-.18||axial>=4.8||radius<1.25||radius>=2.4)continue;
        double ownedDistance=std::numeric_limits<double>::max(),otherDistance=ownedDistance;
        for(const auto& surface:surfaces){
            const double d=length(subtract(point,closest(point,surface.a,surface.b,surface.c)));
            auto& nearest=surface.owned?ownedDistance:otherDistance;
            nearest=std::min(nearest,d);
        }
        if(ownedDistance>.18||ownedDistance>otherDistance+1e-10)continue;
        const double radialWeight=radius<=2.0?1.0:(2.4-radius)/.4;
        const double stopWeight=axial<=4.0?1.0:std::clamp((4.8-axial)/.8,0.0,1.0);
        point=add(point,scaled(radial,correction*.5*radialWeight*stopWeight/radius));
        ++moved;
    }
    if(moved<24||!validatePreparedMesh(analyzeSource(*adjusted)).ok()){
        if(diagnostic)*diagnostic=QStringLiteral("Plain-wheel blind-bore correction failed strict topology validation (%1 vertices).").arg(moved);
        return false;
    }
    if(diagnostic)*diagnostic=QStringLiteral("Certified continuous blind bore adjusted (%1 vertices); closed stop and wheel exterior retained.").arg(moved);
    return true;
}
}

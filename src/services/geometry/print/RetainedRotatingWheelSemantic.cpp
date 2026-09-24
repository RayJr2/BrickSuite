#include "RetainedRotatingWheelSemantic.h"

#include <QCryptographicHash>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point column(const std::array<double,12>& t,int c){return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu};}
Point origin(const std::array<double,12>& t){return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu};}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double length(Point a){return std::sqrt(dot(a,a));}
Point scaled(Point p,double s){return {p.x*s,p.y*s,p.z*s};}
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
}

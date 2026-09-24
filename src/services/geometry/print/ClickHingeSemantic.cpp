#include "ClickHingeSemantic.h"

#include <QCryptographicHash>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point column(const std::array<double,12>& t,int c){return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu};}
Point origin(const std::array<double,12>& t){return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu};}
Point scaled(Point p,double s){return {p.x*s,p.y*s,p.z*s};}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double length(Point p){return std::sqrt(dot(p,p));}
Point cross(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z};}
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
FunctionalFeature feature(const LDrawGeometry::LDrawSourceModel& model,
    const LDrawGeometry::ReferenceRecord& owner,FunctionalInterfaceRole role){
    FunctionalFeature f;
    f.family=FunctionalInterfaceFamily::ClickHinge;
    f.role=role;
    f.materialSide=FunctionalMaterialSide::MaterialInside;
    f.operandAction=FunctionalOperandAction::Unite;
    f.eligibility=FunctionalEligibility::Eligible;
    f.confidence=SemanticConfidence::HighConfidence;
    const auto axis=column(owner.accumulatedTransform,0);
    const auto u=column(owner.accumulatedTransform,1);
    f.frame={origin(owner.accumulatedTransform),scaled(axis,1.0/length(axis)),
             scaled(u,1.0/length(u)),cross(scaled(axis,1.0/length(axis)),
             scaled(u,1.0/length(u))),owner.mirrored};
    // This reference is the hinge bearing, not the calibrated click-force
    // dimension. The arrestor advance is a separate coupled contact offset.
    f.nominalRadiusMillimetres=1.6;
    f.nominalDiameterMillimetres=3.2;
    f.nominalAxialExtentMillimetres=7.2;
    f.nominalEngagementExtentMillimetres=7.2;
    f.radialProfile={{-3.6,1.6},{3.6,1.6}};
    f.constructionRecipe=role==FunctionalInterfaceRole::Male?
        QStringLiteral("click-hinge-clh1-paired-arrestors-v1"):
        QStringLiteral("click-hinge-clh4-paired-indexed-mate-v1");
    f.evidenceContract=QStringLiteral("official-ldraw-clh1-clh4-click-lock-v1");
    f.provenance={{model.files[owner.fileId].relativePath,owner.id,owner.sourceLine,owner.inverted}};
    const auto bytes=QStringLiteral("%1|%2|%3").arg(model.files.front().relativePath)
        .arg(owner.sourceLine).arg(role==FunctionalInterfaceRole::Male?"arrestor":"indexed-mate").toUtf8();
    f.stableIdentity=QStringLiteral("click-hinge:")+
        QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
    f.governingOperandIdentity=f.stableIdentity+QStringLiteral(":detent-contacts");
    return f;
}
} // namespace

QVector<FunctionalFeature> ClickHingeSemantic::recognize(const LDrawGeometry::LDrawLoadResult& source){
    if(!source.ok()||!source.sourceModel)return {};
    const auto& model=*source.sourceModel;
    if(model.references.isEmpty()||model.files.isEmpty()||
       model.files[model.references.front().fileId].classification!=LDrawGeometry::SourceClassification::Part)return {};
    const int root=model.references.front().id;
    const LDrawGeometry::ReferenceRecord* single=nullptr;
    QVector<const LDrawGeometry::ReferenceRecord*> dual;
    for(const auto& ref:model.references)if(ref.parentId==root){
        const auto p=path(model,ref);
        if(p==QStringLiteral("p/clh1.dat")){if(single)return {};single=&ref;}
        else if(p==QStringLiteral("p/clh4.dat"))dual.push_back(&ref);
    }
    if(single&&!dual.isEmpty())return {};
    if(single){
        int quarters=0,arrestorWalls=0;
        for(const auto& ref:model.references)if(ref.parentId==single->id){
            const auto p=path(model,ref);
            if(p==QStringLiteral("p/clh3q.dat")){if(certifiedCount(model,ref.id)<30)return {}; ++quarters;}
            if(p==QStringLiteral("p/8/2-4cyli.dat")){if(certifiedCount(model,ref.id)<8)return {}; ++arrestorWalls;}
        }
        if(quarters!=4||arrestorWalls!=4||certifiedCount(model,single->id)<200)return {};
        return {feature(model,*single,FunctionalInterfaceRole::Male)};
    }
    if(dual.size()!=2)return {};
    for(const auto* half:dual){
        if(certifiedCount(model,half->id)<100)return {};
        int contactArcs=0;
        for(const auto& ref:model.references)if(ref.parentId==half->id&&
            path(model,ref)==QStringLiteral("p/2-4cyli.dat")&&certifiedCount(model,ref.id)>=8)++contactArcs;
        if(contactArcs<3)return {};
    }
    const auto a=column(dual[0]->accumulatedTransform,0),b=column(dual[1]->accumulatedTransform,0);
    if(std::abs(length(a)-.4)>1e-4||std::abs(length(b)-.4)>1e-4||
       dot(a,b)>-.16+1e-4)return {};
    auto indexedMate=feature(model,*dual.front(),FunctionalInterfaceRole::Female);
    indexedMate.provenance.push_back({model.files[dual[1]->fileId].relativePath,
        dual[1]->id,dual[1]->sourceLine,dual[1]->inverted});
    return {indexedMate};
}
}

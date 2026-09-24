#include "BallSocketSemantic.h"

#include <QCryptographicHash>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point column(const std::array<double,12>& t,int c) { return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu}; }
Point origin(const std::array<double,12>& t) { return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Point a) { return std::sqrt(dot(a,a)); }
Point scaled(Point a,double factor) { return {a.x*factor,a.y*factor,a.z*factor}; }
Point cross(Point a,Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
QString path(const LDrawGeometry::LDrawSourceModel& model,const LDrawGeometry::ReferenceRecord& ref) {
    return ref.fileId>=0&&ref.fileId<model.files.size()
        ?model.files[ref.fileId].relativePath.toLower():QString{};
}
bool certifiedDescendants(const LDrawGeometry::LDrawSourceModel& model,int ancestor,int* count) {
    *count=0;
    for(const auto& surface:model.surfaces) {
        int ref=surface.referenceId;
        while(ref>=0&&ref<model.references.size()&&ref!=ancestor)
            ref=model.references[ref].parentId;
        if(ref!=ancestor)continue;
        if(!surface.certified)return false;
        ++*count;
    }
    return true;
}
} // namespace

QVector<FunctionalFeature> BallSocketSemantic::recognize(const LDrawGeometry::LDrawLoadResult& source) {
    if(!source.ok()||!source.sourceModel)return {};
    const auto& model=*source.sourceModel;
    if(model.references.isEmpty()||model.files.isEmpty()||
       model.files[model.references.front().fileId].classification!=LDrawGeometry::SourceClassification::Part)
        return {};
    QVector<FunctionalFeature> result;
    for(const auto& socket:model.references) {
        const QString socketPath=path(model,socket);
        if((socketPath!=QStringLiteral("p/joint8socket1.dat")&&
            socketPath!=QStringLiteral("p/joint8socket3.dat"))||
           socket.parentId!=model.references.front().id)continue;
        const LDrawGeometry::ReferenceRecord* core=&socket;
        if(socketPath==QStringLiteral("p/joint8socket1.dat")) {
            core=nullptr;
            for(const auto& child:model.references)if(child.parentId==socket.id&&
                path(model,child)==QStringLiteral("p/joint8socket3.dat")) {
                if(core){core=nullptr;break;}
                core=&child;
            }
            if(!core)continue;
        }
        const auto& t=core->accumulatedTransform;
        const Point x=column(t,0),y=column(t,1),z=column(t,2);
        if(std::abs(length(x)-MmPerLdu)>1e-4||std::abs(length(y)-MmPerLdu)>1e-4||
           std::abs(length(z)-MmPerLdu)>1e-4||std::abs(dot(x,y))>1e-5||
           std::abs(dot(y,z))>1e-5||std::abs(dot(x,z))>1e-5)continue;
        int coreTriangles=0,socketTriangles=0;
        if(!certifiedDescendants(model,core->id,&coreTriangles)||coreTriangles<150||
           !certifiedDescendants(model,socket.id,&socketTriangles)||socketTriangles<150)continue;
        FunctionalFeature feature;
        feature.family=FunctionalInterfaceFamily::BallSocket;
        feature.role=FunctionalInterfaceRole::Female;
        feature.materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;
        feature.eligibility=FunctionalEligibility::Eligible;
        feature.confidence=SemanticConfidence::HighConfidence;
        // The joint8socket3 mouth points toward local -Z. The retaining arms
        // and concave spherical contact are one certified source mechanism.
        feature.frame={origin(t),scaled(z,-1.0/MmPerLdu),scaled(x,1.0/MmPerLdu),{},socket.mirrored};
        feature.frame.profileV=cross(feature.frame.axis,feature.frame.profileU);
        feature.nominalRadiusMillimetres=3.2;
        feature.nominalDiameterMillimetres=6.4;
        feature.nominalAxialExtentMillimetres=6.4;
        feature.nominalEngagementExtentMillimetres=3.2;
        feature.operandAction=FunctionalOperandAction::Subtract;
        feature.constructionRecipe=QStringLiteral("ball-socket-8-retaining-cup-v1");
        feature.evidenceContract=QStringLiteral("official-ldraw-joint8socket-friction-v1");
        feature.radialProfile={{-3.2,0},{0,3.2},{3.2,0}};
        feature.provenance={{model.files[socket.fileId].relativePath,socket.id,socket.sourceLine,socket.inverted},
                            {model.files[core->fileId].relativePath,core->id,core->sourceLine,core->inverted}};
        const QByteArray identity=QStringLiteral("%1|%2|%3").arg(model.files.front().relativePath)
            .arg(socket.sourceLine).arg(core->sourceLine).toUtf8();
        feature.stableIdentity=QStringLiteral("ball-socket:")+
            QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
        feature.governingOperandIdentity=feature.stableIdentity+QStringLiteral(":spherical-contact-and-throat");
        result.push_back(feature);
    }
    return result;
}
} // namespace PrintGeometry

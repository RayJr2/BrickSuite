#include "BallJointSemantic.h"

#include <QCryptographicHash>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point column(const std::array<double,12>& t,int c) { return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu}; }
Point origin(const std::array<double,12>& t) { return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Point a) { return std::sqrt(dot(a,a)); }
Point scaled(Point a,double value) { return {a.x*value,a.y*value,a.z*value}; }
Point cross(Point a,Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.y}; }
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

QVector<FunctionalFeature> BallJointSemantic::recognize(const LDrawGeometry::LDrawLoadResult& source) {
    if(!source.ok()||!source.sourceModel)return {};
    const auto& model=*source.sourceModel;
    if(model.references.isEmpty()||model.files.isEmpty()||
       model.files[model.references.front().fileId].classification!=LDrawGeometry::SourceClassification::Part)
        return {};
    QVector<FunctionalFeature> result;
    for(const auto& joint:model.references) {
        if(path(model,joint)!=QStringLiteral("p/joint8ball.dat")||
           joint.parentId!=model.references.front().id)continue;
        const LDrawGeometry::ReferenceRecord* sphere=nullptr;
        const LDrawGeometry::ReferenceRecord* stem=nullptr;
        for(const auto& child:model.references) {
            if(child.parentId!=joint.id)continue;
            if(path(model,child)==QStringLiteral("p/8-8sphe.dat")) {
                if(sphere){sphere=nullptr;break;}
                sphere=&child;
            } else if(path(model,child)==QStringLiteral("p/4-4cyl1sph2.dat")) stem=&child;
        }
        if(!sphere||!stem)continue;
        const auto& t=sphere->accumulatedTransform;
        const Point x=column(t,0),y=column(t,1),z=column(t,2);
        if(std::abs(length(x)-3.2)>1e-3||std::abs(length(y)-3.2)>1e-3||
           std::abs(length(z)-3.2)>1e-3||std::abs(dot(x,y))>1e-3||
           std::abs(dot(y,z))>1e-3||std::abs(dot(x,z))>1e-3)continue;
        int sphereTriangles=0,jointTriangles=0;
        if(!certifiedDescendants(model,sphere->id,&sphereTriangles)||sphereTriangles<64||
           !certifiedDescendants(model,joint.id,&jointTriangles)||jointTriangles<80)continue;
        FunctionalFeature feature;
        feature.family=FunctionalInterfaceFamily::BallJoint;
        feature.role=FunctionalInterfaceRole::Male;
        feature.materialSide=FunctionalMaterialSide::MaterialInside;
        feature.eligibility=FunctionalEligibility::Eligible;
        feature.confidence=SemanticConfidence::HighConfidence;
        feature.frame={origin(t),scaled(y,1.0/length(y)),scaled(x,1.0/length(x)),{},joint.mirrored};
        feature.frame.profileV=cross(feature.frame.axis,feature.frame.profileU);
        feature.nominalRadiusMillimetres=3.2;
        feature.nominalDiameterMillimetres=6.4;
        feature.nominalAxialExtentMillimetres=6.4;
        feature.nominalEngagementExtentMillimetres=6.4;
        feature.operandAction=FunctionalOperandAction::Unite;
        feature.constructionRecipe=QStringLiteral("ball-joint-8-spherical-head-v1");
        feature.evidenceContract=QStringLiteral("official-ldraw-joint8ball-sphere-v1");
        feature.radialProfile={{-3.2,0},{0,3.2},{3.2,0}};
        feature.provenance={{model.files[joint.fileId].relativePath,joint.id,joint.sourceLine,joint.inverted},
                            {model.files[sphere->fileId].relativePath,sphere->id,sphere->sourceLine,sphere->inverted}};
        const QByteArray identity=QStringLiteral("%1|%2|%3").arg(model.files.front().relativePath)
            .arg(joint.sourceLine).arg(sphere->sourceLine).toUtf8();
        feature.stableIdentity=QStringLiteral("ball-joint:")+
            QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
        feature.governingOperandIdentity=feature.stableIdentity+QStringLiteral(":spherical-head");
        result.push_back(feature);
    }
    return result;
}
} // namespace PrintGeometry

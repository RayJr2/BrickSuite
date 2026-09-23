#include "CClipBarReceiverSemantic.h"

#include <QCryptographicHash>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point column(const std::array<double,12>& t,int c) { return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu}; }
Point origin(const std::array<double,12>& t) { return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Point a) { return std::sqrt(dot(a,a)); }
Point scaled(Point a,double s) { return {a.x*s,a.y*s,a.z*s}; }
Point cross(Point a,Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z}; }
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
} // namespace PrintGeometry

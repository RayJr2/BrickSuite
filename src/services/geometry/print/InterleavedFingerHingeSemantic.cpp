#include "InterleavedFingerHingeSemantic.h"

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
QString path(const LDrawGeometry::LDrawSourceModel& model,const LDrawGeometry::ReferenceRecord& ref) {
    return ref.fileId>=0&&ref.fileId<model.files.size()?model.files[ref.fileId].relativePath.toLower():QString{};
}
bool descendant(const LDrawGeometry::LDrawSourceModel& model,int reference,int ancestor) {
    while(reference>=0&&reference<model.references.size()&&reference!=ancestor)
        reference=model.references[reference].parentId;
    return reference==ancestor;
}
int certifiedCount(const LDrawGeometry::LDrawSourceModel& model,int ancestor) {
    int count=0;
    for(const auto& surface:model.surfaces)if(descendant(model,surface.referenceId,ancestor)) {
        if(!surface.certified)return -1;
        ++count;
    }
    return count;
}
} // namespace

QVector<FunctionalFeature> InterleavedFingerHingeSemantic::recognize(const LDrawGeometry::LDrawLoadResult& source) {
    if(!source.ok()||!source.sourceModel)return {};
    const auto& model=*source.sourceModel;
    if(model.references.isEmpty()||model.files.isEmpty()||
       model.files[model.references.front().fileId].classification!=LDrawGeometry::SourceClassification::Part)
        return {};
    const LDrawGeometry::ReferenceRecord* hinge=nullptr;
    bool threeFinger=false;
    for(const auto& ref:model.references) {
        const auto p=path(model,ref);
        if(p!=QStringLiteral("p/h1.dat")&&p!=QStringLiteral("p/h2.dat"))continue;
        if(hinge)return {};
        hinge=&ref;threeFinger=p==QStringLiteral("p/h2.dat");
    }
    if(!hinge||certifiedCount(model,hinge->id)<180)return {};
    int bumps=0,halfCylinders=0,outerWalls=0;
    for(const auto& ref:model.references)if(ref.parentId==hinge->id) {
        const auto p=path(model,ref);
        if(p==QStringLiteral("p/bump5000.dat")) {
            if(certifiedCount(model,ref.id)<48||
               std::abs(length(column(ref.accumulatedTransform,1))-.6)>.001)return {};
            ++bumps;
        } else if(p==QStringLiteral("p/2-4cylc.dat")) {
            if(certifiedCount(model,ref.id)<16)return {};
            ++halfCylinders;
        } else if(p==QStringLiteral("p/2-4cylo.dat")||p==QStringLiteral("p/2-4cyli.dat")) {
            if(certifiedCount(model,ref.id)<16)return {};
            ++outerWalls;
        }
    }
    if(bumps!=2||outerWalls!=(threeFinger?2:1)||halfCylinders!=(threeFinger?3:4))return {};
    const auto& t=hinge->accumulatedTransform;
    const auto axis=column(t,2),u=column(t,0);
    if(std::abs(length(axis)-.4)>1e-4||std::abs(length(u)-.4)>1e-4||
       std::abs(dot(axis,u))>1e-5)return {};
    FunctionalFeature f;
    f.family=FunctionalInterfaceFamily::InterleavedFingerHinge;
    f.role=threeFinger?FunctionalInterfaceRole::Male:FunctionalInterfaceRole::Female;
    f.materialSide=FunctionalMaterialSide::MaterialInside;
    f.operandAction=FunctionalOperandAction::Unite;
    f.eligibility=FunctionalEligibility::Eligible;
    f.confidence=SemanticConfidence::HighConfidence;
    f.frame={origin(t),scaled(axis,1.0/length(axis)),scaled(u,1.0/length(u)),
             cross(scaled(axis,1.0/length(axis)),scaled(u,1.0/length(u))),hinge->mirrored};
    f.nominalRadiusMillimetres=1.6;
    f.nominalDiameterMillimetres=3.2;
    f.nominalAxialExtentMillimetres=8.0;
    f.nominalEngagementExtentMillimetres=8.0;
    f.radialProfile={{-4.0,1.6},{4.0,1.6}};
    f.constructionRecipe=threeFinger?QStringLiteral("interleaved-hinge-three-finger-bump-v1"):
        QStringLiteral("interleaved-hinge-two-finger-mate-v1");
    f.evidenceContract=QStringLiteral("official-ldraw-h1-h2-interleaved-finger-v1");
    f.provenance={{model.files[hinge->fileId].relativePath,hinge->id,hinge->sourceLine,hinge->inverted}};
    const auto identity=QStringLiteral("%1|%2|%3").arg(model.files.front().relativePath)
        .arg(hinge->sourceLine).arg(threeFinger?"three":"two").toUtf8();
    f.stableIdentity=QStringLiteral("interleaved-finger-hinge:")+
        QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
    f.governingOperandIdentity=f.stableIdentity+QStringLiteral(":contact-bumps");
    return {f};
}
}

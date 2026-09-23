#include "StandardBarSemantic.h"

#include <QCryptographicHash>
#include <QRegularExpression>
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
} // namespace PrintGeometry

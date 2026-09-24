#include "PinBarrelHingeSemantic.h"
#include "PrintMeshAnalysis.h"

#include <QCryptographicHash>
#include <cmath>
#include <algorithm>
#include <limits>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point column(const std::array<double,12>& t,int c) { return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu}; }
Point origin(const std::array<double,12>& t) { return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu}; }
Point scaled(Point a,double s) { return {a.x*s,a.y*s,a.z*s}; }
Point add(Point a,Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Point subtract(Point a,Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Point a) { return std::sqrt(dot(a,a)); }
Point cross(Point a,Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.y}; }
Point converted(const QVector3D& p) { return {p.x()*MmPerLdu,p.z()*MmPerLdu,-p.y()*MmPerLdu}; }
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
QString path(const LDrawGeometry::LDrawSourceModel& model,const LDrawGeometry::ReferenceRecord& ref) {
    return ref.fileId>=0&&ref.fileId<model.files.size()?model.files[ref.fileId].relativePath.toLower():QString{};
}
bool descendant(const LDrawGeometry::LDrawSourceModel& model,int reference,int ancestor) {
    while(reference>=0&&reference<model.references.size()&&reference!=ancestor)
        reference=model.references[reference].parentId;
    return reference==ancestor;
}
bool certified(const LDrawGeometry::LDrawSourceModel& model,int ancestor,int minimum) {
    int count=0;
    for(const auto& surface:model.surfaces)if(descendant(model,surface.referenceId,ancestor)) {
        if(!surface.certified)return false;
        ++count;
    }
    return count>=minimum;
}
bool orthogonal(const std::array<double,12>& t,double radius) {
    const auto x=column(t,0),y=column(t,1),z=column(t,2);
    return std::abs(length(x)-radius)<1e-4&&std::abs(length(y)-radius)<1e-4&&
        std::abs(length(z)-radius)<1e-4&&std::abs(dot(x,y))<1e-5&&
        std::abs(dot(x,z))<1e-5&&std::abs(dot(y,z))<1e-5;
}
FunctionalFeature feature(const LDrawGeometry::LDrawSourceModel& model,
    const LDrawGeometry::ReferenceRecord& owner,const LDrawGeometry::ReferenceRecord& contact,
    FunctionalInterfaceRole role) {
    const auto& t=contact.accumulatedTransform;
    const Point axis=scaled(column(t,1),1.0/length(column(t,1)));
    const Point u=scaled(column(t,0),1.0/length(column(t,0)));
    FunctionalFeature f;
    f.family=FunctionalInterfaceFamily::PinBarrelHinge;
    f.role=role;
    f.materialSide=role==FunctionalInterfaceRole::Male?FunctionalMaterialSide::MaterialInside:
        FunctionalMaterialSide::EmptyInsideMaterialOutside;
    f.operandAction=role==FunctionalInterfaceRole::Male?FunctionalOperandAction::Unite:
        FunctionalOperandAction::Subtract;
    f.eligibility=FunctionalEligibility::Eligible;
    f.confidence=SemanticConfidence::HighConfidence;
    f.frame={origin(t),axis,u,cross(axis,u),contact.mirrored};
    f.nominalRadiusMillimetres=1.6;
    f.nominalDiameterMillimetres=3.2;
    f.nominalAxialExtentMillimetres=1.6;
    f.nominalEngagementExtentMillimetres=1.6;
    if(role==FunctionalInterfaceRole::Male)f.protectedInnerRadiusMillimetres=.8;
    f.radialProfile={{0,1.6},{1.6,1.6}};
    f.constructionRecipe=role==FunctionalInterfaceRole::Male?
        QStringLiteral("pin-barrel-hinge-hollow-pin-v1"):
        QStringLiteral("pin-barrel-hinge-open-retaining-barrel-v1");
    f.evidenceContract=QStringLiteral("official-ldraw-3937-3938-rotating-pair-v1");
    f.provenance={{model.files[owner.fileId].relativePath,owner.id,owner.sourceLine,owner.inverted},
                  {model.files[contact.fileId].relativePath,contact.id,contact.sourceLine,contact.inverted}};
    const auto identity=QStringLiteral("%1|%2|%3|%4").arg(model.files.front().relativePath)
        .arg(owner.sourceLine).arg(contact.sourceLine).arg(role==FunctionalInterfaceRole::Male?"male":"female").toUtf8();
    f.stableIdentity=QStringLiteral("pin-barrel-hinge:")+
        QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
    f.governingOperandIdentity=f.stableIdentity+QStringLiteral(":contact");
    return f;
}
} // namespace

QVector<FunctionalFeature> PinBarrelHingeSemantic::recognize(const LDrawGeometry::LDrawLoadResult& source) {
    if(!source.ok()||!source.sourceModel)return {};
    const auto& model=*source.sourceModel;
    if(model.references.isEmpty()||model.files.isEmpty()||
       model.files[model.references.front().fileId].classification!=LDrawGeometry::SourceClassification::Part)
        return {};
    const int root=model.references.front().id;
    QVector<FunctionalFeature> result;
    // 3938 has two certified, mirrored hinge-half subparts. Each owns a full
    // OD cylinder and a separate 1.60 mm bore. Both are required: an ordinary
    // 3.20 mm rod must never be mistaken for the hinge pin.
    QVector<const LDrawGeometry::ReferenceRecord*> halves;
    for(const auto& ref:model.references)if(ref.parentId==root&&path(model,ref)==QStringLiteral("parts/s/3938s01.dat"))
        halves.push_back(&ref);
    if(halves.size()==2&&halves[0]->mirrored!=halves[1]->mirrored&&
       std::abs(origin(halves[0]->accumulatedTransform).x+
                origin(halves[1]->accumulatedTransform).x)<1e-4)for(const auto* half:halves) {
        const LDrawGeometry::ReferenceRecord *outer=nullptr,*bore=nullptr;
        for(const auto& ref:model.references)if(ref.parentId==half->id&&
            path(model,ref)==QStringLiteral("p/4-4cylo.dat")) {
            const auto& t=ref.accumulatedTransform;
            const double radius=length(column(t,0));
            if(std::abs(radius-1.6)<1e-4){if(outer)outer=nullptr;else outer=&ref;}
            else if(std::abs(radius-.8)<1e-4)bore=&ref;
        }
        if(!outer||!bore||!certified(model,half->id,80)||
           !certified(model,outer->id,16))continue;
        result.push_back(feature(model,*half,*outer,FunctionalInterfaceRole::Male));
    }
    // The 3937 base has two open retaining barrels, each split into two
    // certified 5/16-circle walls. The pair, not an isolated arc, identifies
    // a rotational snap fit. The mouth relief remains separate source geometry.
    QVector<const LDrawGeometry::ReferenceRecord*> arcs;
    QVector<const LDrawGeometry::ReferenceRecord*> mouths;
    for(const auto& ref:model.references)if(ref.parentId==root&&
        path(model,ref)==QStringLiteral("p/5-16cylo.dat")&&certified(model,ref.id,8))arcs.push_back(&ref);
    for(const auto& ref:model.references)if(ref.parentId==root&&
        path(model,ref).endsWith(QStringLiteral("3-8cylo.dat"))&&certified(model,ref.id,6))mouths.push_back(&ref);
    if(arcs.size()==4&&mouths.size()==2&&
       std::abs(std::abs(origin(arcs[0]->accumulatedTransform).x-
                         origin(arcs[2]->accumulatedTransform).x)-12.8)<1e-4)
       for(int i=0;i<arcs.size();i+=2) {
        const auto& a=*arcs[i];const auto& b=*arcs[i+1];
        const auto p=origin(a.accumulatedTransform),q=origin(b.accumulatedTransform);
        if(std::abs(p.x-q.x)>1e-4||std::abs(p.y-q.y)>1e-4||std::abs(p.z-q.z)>1e-4||
           !orthogonal(a.accumulatedTransform,1.6)||!orthogonal(b.accumulatedTransform,1.6)||
           std::abs(p.x-origin(mouths[i/2]->accumulatedTransform).x)>1e-4)continue;
        result.push_back(feature(model,model.references.front(),a,FunctionalInterfaceRole::Female));
    }
    return result.size()==2?result:QVector<FunctionalFeature>{};
}

bool PinBarrelHingeSemantic::adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
    const PrintMesh& nominal,const FunctionalFeature& pin,double correction,
    PrintMesh* adjusted,QString* diagnostic) {
    if(!adjusted||!source.ok()||!source.sourceModel||nominal.faces.empty()||
       pin.family!=FunctionalInterfaceFamily::PinBarrelHinge||pin.role!=FunctionalInterfaceRole::Male||
       pin.constructionRecipe!=QStringLiteral("pin-barrel-hinge-hollow-pin-v1")||
       pin.evidenceContract!=QStringLiteral("official-ldraw-3937-3938-rotating-pair-v1")||
       pin.provenance.size()!=2||!std::isfinite(correction)||std::abs(correction)>.25||
       1.6+correction*.5<=pin.protectedInnerRadiusMillimetres) {
        if(diagnostic)*diagnostic=QStringLiteral("The certified hinge pin correction or source contract is invalid.");
        return false;
    }
    const auto& model=*source.sourceModel;
    const int owner=pin.provenance.back().referenceId;
    if(owner<0||owner>=model.references.size()||path(model,model.references[owner])!=QStringLiteral("p/4-4cylo.dat")){
        if(diagnostic)*diagnostic=QStringLiteral("The certified hinge pin exterior primitive is missing.");return false;
    }
    struct Triangle {Point a,b,c;};
    QVector<Triangle> walls;
    for(const auto& surface:model.surfaces)if(descendant(model,surface.referenceId,owner)) {
        if(!surface.certified||surface.triangleIndex<0||surface.triangleIndex>=source.mesh.triangles.size()){
            if(diagnostic)*diagnostic=QStringLiteral("The hinge pin exterior has uncertified source triangles.");return false;
        }
        const auto& t=source.mesh.triangles[surface.triangleIndex];
        walls.push_back({converted(t.a),converted(t.b),converted(t.c)});
    }
    if(walls.size()<16){if(diagnostic)*diagnostic=QStringLiteral("Only %1 certified hinge exterior triangles were retained.").arg(walls.size());return false;}
    if(correction==0.0){*adjusted=nominal;return true;}
    struct Selection {std::size_t index;double distance;};
    QVector<Selection> selected;
    double measuredRadius=0;
    for(std::size_t i=0;i<nominal.vertices.size();++i) {
        const auto relative=subtract(nominal.vertices[i],pin.frame.origin);
        const double axial=dot(relative,pin.frame.axis);
        const auto radial=subtract(relative,scaled(pin.frame.axis,axial));
        const double radius=length(radial);
        if(axial<-.02||axial>pin.nominalAxialExtentMillimetres+.02||
           std::abs(radius-pin.nominalRadiusMillimetres)>.12)continue;
        double distance=std::numeric_limits<double>::max();
        for(const auto& wall:walls)distance=std::min(distance,
            length(subtract(nominal.vertices[i],closest(nominal.vertices[i],wall.a,wall.b,wall.c))));
        if(distance>.12)continue;
        selected.push_back({i,distance});
        if(axial>.25&&axial<1.35)measuredRadius=std::max(measuredRadius,radius);
    }
    if(selected.size()<24||measuredRadius<1.45||measuredRadius>1.7) {
        if(diagnostic)*diagnostic=QStringLiteral("The certified hinge pin exterior was not retained in PreparedMesh.");
        return false;
    }
    *adjusted=nominal;
    const double delta=pin.nominalRadiusMillimetres+correction*.5-measuredRadius;
    const auto smooth=[](double x){x=std::clamp(x,0.0,1.0);return x*x*(3.0-2.0*x);};
    for(const auto& selection:selected) {
        auto& point=adjusted->vertices[selection.index];
        const auto relative=subtract(point,pin.frame.origin);
        const double axial=dot(relative,pin.frame.axis);
        const auto radial=subtract(relative,scaled(pin.frame.axis,axial));
        const double radius=length(radial);
        const double weight=smooth(axial/.20)*smooth((pin.nominalAxialExtentMillimetres-axial)/.20)*
            smooth((radius-1.43)/.12)*smooth((.12-selection.distance)/.10);
        if(radius>1e-9)point=add(point,scaled(radial,delta*weight/radius));
    }
    const auto analysis=analyzeSource(*adjusted);
    if(!validatePreparedMesh(analysis).ok()) {
        if(diagnostic)*diagnostic=QStringLiteral("The adjusted hinge pin failed manifold validation: %1 boundary, %2 non-manifold, %3 self-intersections; %4 of %5 vertices selected, measured radius %6.")
            .arg(analysis.boundaryEdges).arg(analysis.nonManifoldEdges).arg(analysis.selfIntersections)
            .arg(selected.size()).arg(nominal.vertices.size()).arg(measuredRadius,0,'f',4);
        return false;
    }
    if(diagnostic)*diagnostic=QStringLiteral("Certified hollow hinge pin exterior adjusted to %1 mm OD; 1.60 mm bore and attachment retained (%2 vertices).")
        .arg(pin.nominalDiameterMillimetres+correction,0,'f',3).arg(selected.size());
    return true;
}
}

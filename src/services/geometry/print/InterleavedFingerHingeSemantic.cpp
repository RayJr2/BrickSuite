#include "InterleavedFingerHingeSemantic.h"
#include "PrintMeshAnalysis.h"

#include <QCryptographicHash>
#include <cmath>
#include <algorithm>
#include <limits>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point column(const std::array<double,12>& t,int c) { return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu}; }
Point origin(const std::array<double,12>& t) { return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Point a) { return std::sqrt(dot(a,a)); }
Point scaled(Point a,double s) { return {a.x*s,a.y*s,a.z*s}; }
Point add(Point a,Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Point subtract(Point a,Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Point cross(Point a,Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z}; }
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

bool InterleavedFingerHingeSemantic::adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
    const PrintMesh& nominal,const FunctionalFeature& hinge,double correction,
    PrintMesh* adjusted,QString* diagnostic) {
    if(!adjusted||!source.ok()||!source.sourceModel||nominal.faces.empty()||
       hinge.family!=FunctionalInterfaceFamily::InterleavedFingerHinge||
       hinge.role!=FunctionalInterfaceRole::Male||
       hinge.constructionRecipe!=QStringLiteral("interleaved-hinge-three-finger-bump-v1")||
       hinge.evidenceContract!=QStringLiteral("official-ldraw-h1-h2-interleaved-finger-v1")||
       hinge.provenance.size()!=1||!std::isfinite(correction)||
       .3+correction<=0||std::abs(correction)>.15) {
        if(diagnostic)*diagnostic=QStringLiteral("The certified three-finger contact-bump correction is invalid.");
        return false;
    }
    const auto& model=*source.sourceModel;
    const int owner=hinge.provenance.front().referenceId;
    if(owner<0||owner>=model.references.size()||path(model,model.references[owner])!=QStringLiteral("p/h2.dat"))return false;
    struct Triangle {Point a,b,c;};
    struct Bump {Point base,tip;QVector<Triangle> faces;};
    QVector<Bump> bumps;
    for(const auto& ref:model.references)if(ref.parentId==owner&&path(model,ref)==QStringLiteral("p/bump5000.dat")) {
        const auto axial=column(ref.accumulatedTransform,1);
        if(std::abs(length(axial)-.6)>.001)return false;
        Bump bump{origin(ref.accumulatedTransform),scaled(axial,-1.0/length(axial)),{}};
        for(const auto& surface:model.surfaces)if(descendant(model,surface.referenceId,ref.id)) {
            if(!surface.certified||surface.triangleIndex<0||surface.triangleIndex>=source.mesh.triangles.size())return false;
            const auto& t=source.mesh.triangles[surface.triangleIndex];
            bump.faces.push_back({converted(t.a),converted(t.b),converted(t.c)});
        }
        if(bump.faces.size()<48)return false;
        bumps.push_back(std::move(bump));
    }
    if(bumps.size()!=2){if(diagnostic)*diagnostic=QStringLiteral("Both certified contact bumps are required.");return false;}
    *adjusted=nominal;
    if(correction==0.0)return true;
    int moved=0;
    int movedByBump[2]={0,0};
    for(std::size_t i=0;i<nominal.vertices.size();++i) {
        const auto p=nominal.vertices[i];
        double nearest=std::numeric_limits<double>::max();
        int index=-1;
        double height=0;
        for(int j=0;j<bumps.size();++j) {
            const auto& bump=bumps[j];
            const double h=dot(subtract(p,bump.base),bump.tip);
            if(h<-.01||h>.31)continue;
            for(const auto& face:bump.faces) {
                const double distance=length(subtract(p,closest(p,face.a,face.b,face.c)));
                if(distance<nearest){nearest=distance;index=j;height=h;}
            }
        }
        if(index<0||nearest>.06)continue;
        const double weight=std::clamp(height/.3,0.0,1.0)*
            std::clamp((.06-nearest)/.04,0.0,1.0);
        if(weight<=0)continue;
        adjusted->vertices[i]=add(p,scaled(bumps[index].tip,correction*weight));
        ++moved;
        ++movedByBump[index];
    }
    if(moved<24||movedByBump[0]<12||movedByBump[1]<12){
        if(diagnostic)*diagnostic=QStringLiteral("Both certified contact-bump surfaces must be retained in PreparedMesh.");
        return false;
    }
    const auto analysis=analyzeSource(*adjusted);
    if(!validatePreparedMesh(analysis).ok()) {
        if(diagnostic)*diagnostic=QStringLiteral("Adjusted contact bumps failed manifold validation.");
        return false;
    }
    if(diagnostic)*diagnostic=QStringLiteral("Two certified contact bumps adjusted by %1 mm; fingers and attachment retained (%2 vertices).")
        .arg(correction,0,'f',3).arg(moved);
    return true;
}
}

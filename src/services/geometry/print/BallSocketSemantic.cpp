#include "BallSocketSemantic.h"
#include "PrintMeshAnalysis.h"

#include <QCryptographicHash>
#include <algorithm>
#include <cmath>
#include <limits>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point column(const std::array<double,12>& t,int c) { return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu}; }
Point origin(const std::array<double,12>& t) { return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Point a) { return std::sqrt(dot(a,a)); }
Point scaled(Point a,double factor) { return {a.x*factor,a.y*factor,a.z*factor}; }
Point add(Point a,Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Point subtract(Point a,Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Point converted(const QVector3D& p) { return {double(p.x())*MmPerLdu,double(p.z())*MmPerLdu,-double(p.y())*MmPerLdu}; }
Point cross(Point a,Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
double distanceSquared(Point a,Point b) { const auto d=subtract(a,b);return dot(d,d); }
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
bool descendant(const LDrawGeometry::LDrawSourceModel& model,int reference,int ancestor) {
    while(reference>=0&&reference<model.references.size()&&reference!=ancestor)
        reference=model.references[reference].parentId;
    return reference==ancestor;
}
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

bool BallSocketSemantic::adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
    const PrintMesh& nominal,const FunctionalFeature& socket,double correction,
    PrintMesh* adjusted,QString* diagnostic) {
    if(!adjusted||!source.ok()||!source.sourceModel||nominal.faces.empty()||
       socket.family!=FunctionalInterfaceFamily::BallSocket||
       socket.constructionRecipe!=QStringLiteral("ball-socket-8-retaining-cup-v1")||
       socket.evidenceContract!=QStringLiteral("official-ldraw-joint8socket-friction-v1")||
       socket.provenance.size()!=2||!std::isfinite(correction)||std::abs(correction)>.4) {
        if(diagnostic)*diagnostic=QStringLiteral("The certified Ball Socket contact/throat correction is invalid.");
        return false;
    }
    const auto& model=*source.sourceModel;
    const int owner=socket.provenance.back().referenceId;
    if(owner<0||owner>=model.references.size()||
       path(model,model.references[owner])!=QStringLiteral("p/joint8socket3.dat")) {
        if(diagnostic)*diagnostic=QStringLiteral("The certified friction-socket source owner is missing.");
        return false;
    }
    if(correction==0.0) {
        *adjusted=nominal;
        if(diagnostic)*diagnostic=QStringLiteral("Verified zero Ball Socket correction retained the nominal PreparedMesh exactly.");
        return true;
    }
    struct Surface {Point a,b,c;bool fit=false;};
    QVector<Surface> surfaces;surfaces.reserve(model.surfaces.size());
    int contactFaces=0,mouthFaces=0;
    for(const auto& surface:model.surfaces) {
        if(!surface.certified||surface.triangleIndex<0||
           surface.triangleIndex>=source.mesh.triangles.size()) {
            if(diagnostic)*diagnostic=QStringLiteral("Ball Socket source-surface certification is incomplete.");
            return false;
        }
        const auto& triangle=source.mesh.triangles[surface.triangleIndex];
        const Point p[3]={converted(triangle.a),converted(triangle.b),converted(triangle.c)};
        const Point centroid=scaled(add(add(p[0],p[1]),p[2]),1.0/3.0);
        const auto from=subtract(centroid,socket.frame.origin);
        const auto normal=cross(subtract(p[1],p[0]),subtract(p[2],p[0]));
        const bool owned=descendant(model,surface.referenceId,owner);
        const bool contact=owned&&length(from)>=3.04&&length(from)<=3.25&&dot(normal,from)<0;
        bool mouth=false;
        if(owned)for(const auto& point:p) {
            const auto relative=subtract(point,socket.frame.origin);
            const double axial=dot(relative,socket.frame.axis);
            const double radius=length(subtract(relative,scaled(socket.frame.axis,axial)));
            if(axial>=1.55&&axial<=2.7&&radius>=2.35&&radius<=3.35)mouth=true;
        }
        contactFaces+=contact;mouthFaces+=mouth;
        surfaces.push_back({p[0],p[1],p[2],contact||mouth});
    }
    if(contactFaces<40||mouthFaces<12) {
        if(diagnostic)*diagnostic=QStringLiteral("Certified Ball Socket contact or retaining-mouth ownership is incomplete.");
        return false;
    }
    PrintMesh result=nominal;
    int movedContact=0,movedMouth=0;
    for(auto& point:result.vertices) {
        const auto relative=subtract(point,socket.frame.origin);
        const double axial=dot(relative,socket.frame.axis);
        const auto transverse=subtract(relative,scaled(socket.frame.axis,axial));
        const double radial=length(transverse),spherical=length(relative);
        const bool mouth=axial>=1.55&&axial<=2.7&&radial>=2.35&&radial<=3.35;
        const bool contact=spherical>=3.04&&spherical<=3.25;
        if(!mouth&&!contact)continue;
        double fitDistance=std::numeric_limits<double>::max(),otherDistance=fitDistance;
        for(const auto& surface:surfaces) {
            const double distance=distanceSquared(point,closest(point,surface.a,surface.b,surface.c));
            auto& nearest=surface.fit?fitDistance:otherDistance;
            nearest=std::min(nearest,distance);
        }
        if(fitDistance>.12*.12||fitDistance>otherDistance+1e-10)continue;
        if(mouth) {
            point=add(point,scaled(transverse,correction/(2.0*radial)));
            ++movedMouth;
        } else {
            point=add(point,scaled(relative,correction/(2.0*spherical)));
            ++movedContact;
        }
    }
    const auto analysis=analyzeSource(result);
    if(movedContact<24||movedMouth<12||!validatePreparedMesh(analysis).ok()) {
        if(diagnostic)*diagnostic=QStringLiteral("The source-owned Ball Socket contact/throat correction failed strict topology validation (%1 contact, %2 mouth vertices moved).")
            .arg(movedContact).arg(movedMouth);
        return false;
    }
    *adjusted=std::move(result);
    if(diagnostic)*diagnostic=QStringLiteral("Certified friction-socket spherical contact and retaining mouth adjusted together; unrelated surfaces retained (%1 contact, %2 mouth vertices moved).")
        .arg(movedContact).arg(movedMouth);
    return true;
}
} // namespace PrintGeometry

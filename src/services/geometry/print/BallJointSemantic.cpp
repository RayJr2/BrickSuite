#include "BallJointSemantic.h"
#include "PrintMeshAnalysis.h"

#include <QCryptographicHash>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point column(const std::array<double,12>& t,int c) { return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu}; }
Point origin(const std::array<double,12>& t) { return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(Point a) { return std::sqrt(dot(a,a)); }
Point scaled(Point a,double value) { return {a.x*value,a.y*value,a.z*value}; }
Point add(Point a,Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Point subtract(Point a,Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Point converted(const QVector3D& p) { return {double(p.x())*MmPerLdu,double(p.z())*MmPerLdu,-double(p.y())*MmPerLdu}; }
QString vertexKey(Point p) {
    return QStringLiteral("%1|%2|%3").arg(std::llround(p.x*10000.0))
        .arg(std::llround(p.y*10000.0)).arg(std::llround(p.z*10000.0));
}
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
        // joint8ball's bar exits along its local Z axis; the sphere itself is
        // rotationally symmetric, but print-orientation evidence is not.
        feature.frame={origin(t),scaled(z,1.0/length(z)),scaled(x,1.0/length(x)),{},joint.mirrored};
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

bool BallJointSemantic::adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
    const PrintMesh& nominal, const FunctionalFeature& ball, double correction,
    PrintMesh* adjusted, QString* diagnostic) {
    if(!adjusted||!source.ok()||!source.sourceModel||nominal.faces.empty()||
       ball.family!=FunctionalInterfaceFamily::BallJoint||
       ball.constructionRecipe!=QStringLiteral("ball-joint-8-spherical-head-v1")||
       ball.evidenceContract!=QStringLiteral("official-ldraw-joint8ball-sphere-v1")||
       ball.provenance.size()!=2||!std::isfinite(correction)||std::abs(correction)>0.8||
       ball.nominalRadiusMillimetres+correction*.5<=0) {
        if(diagnostic)*diagnostic=QStringLiteral("The Ball Joint correction or certified source contract is invalid.");
        return false;
    }
    const auto& model=*source.sourceModel;
    const int owner=ball.provenance[1].referenceId;
    if(owner<0||owner>=model.references.size()||
       path(model,model.references[owner])!=QStringLiteral("p/8-8sphe.dat")) {
        if(diagnostic)*diagnostic=QStringLiteral("The certified Ball Joint spherical owner is missing.");
        return false;
    }
    if(correction==0.0) {
        *adjusted=nominal;
        if(diagnostic)*diagnostic=QStringLiteral("Verified zero Ball Joint correction retained the nominal PreparedMesh exactly.");
        return true;
    }
    struct Surface {Point a,b,c;bool sphere=false;};
    QVector<Surface> surfaces;
    surfaces.reserve(model.surfaces.size());
    QSet<QString> sphereVertices,otherVertices;
    int certifiedTriangles=0;
    for(const auto& surface:model.surfaces) {
        if(!surface.certified||surface.triangleIndex<0||
           surface.triangleIndex>=source.mesh.triangles.size()) {
            if(diagnostic)*diagnostic=QStringLiteral("Ball Joint source-surface certification is incomplete.");
            return false;
        }
        int ref=surface.referenceId;
        while(ref>=0&&ref<model.references.size()&&ref!=owner)ref=model.references[ref].parentId;
        const bool sphereOwned=ref==owner;
        const auto& triangle=source.mesh.triangles[surface.triangleIndex];
        surfaces.push_back({converted(triangle.a),converted(triangle.b),converted(triangle.c),sphereOwned});
        for(const auto& vertex:{triangle.a,triangle.b,triangle.c})
            (sphereOwned?sphereVertices:otherVertices).insert(vertexKey(converted(vertex)));
        if(sphereOwned)++certifiedTriangles;
    }
    if(certifiedTriangles<64||sphereVertices.size()<64) {
        if(diagnostic)*diagnostic=QStringLiteral("The certified Ball Joint spherical source surface is incomplete.");
        return false;
    }
    PrintMesh result=nominal;
    int moved=0;
    for(auto& point:result.vertices) {
        const QString key=vertexKey(point);
        if(otherVertices.contains(key))continue;
        const auto radial=subtract(point,ball.frame.origin);
        const double radius=length(radial);
        if(radius<2.7||radius>3.25)continue;
        double sphereDistance=std::numeric_limits<double>::max();
        double otherDistance=std::numeric_limits<double>::max();
        for(const auto& surface:surfaces) {
            const double d=distanceSquared(point,closest(point,surface.a,surface.b,surface.c));
            auto& nearest=surface.sphere?sphereDistance:otherDistance;
            nearest=std::min(nearest,d);
        }
        if((!sphereVertices.contains(key)&&sphereDistance>0.12*0.12)||
           sphereDistance>otherDistance+1e-10||otherDistance<0.4*0.4)continue;
        const double along=dot(radial,ball.frame.axis);
        const double crossRadius=length(subtract(radial,scaled(ball.frame.axis,along)));
        // The certified spherical belt controls socket fit. Blend to the
        // unchanged stem junction on the bar-facing hemisphere so shrinking
        // the ball cannot fold the stem/sphere interface into itself.
        const double blend=along>0.0 ? std::clamp((crossRadius-1.6)/1.4,0.0,1.0) : 1.0;
        if(blend==0.0)continue;
        point=add(point,scaled(radial,correction*blend/(2.0*radius)));
        ++moved;
    }
    const auto analysis=analyzeSource(result);
    const auto validation=validatePreparedMesh(analysis);
    if(moved<32||!validation.ok()) {
        if(diagnostic)*diagnostic=QStringLiteral("The source-owned Ball Joint sphere correction failed strict topology validation (%1 vertices moved): %2.")
            .arg(moved).arg(QString::fromStdString(validation.message));
        return false;
    }
    *adjusted=std::move(result);
    if(diagnostic)*diagnostic=QStringLiteral("Certified spherical surface adjusted; stem and unrelated source surfaces retained (%1 vertices moved).").arg(moved);
    return true;
}
} // namespace PrintGeometry

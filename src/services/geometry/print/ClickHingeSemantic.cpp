#include "ClickHingeSemantic.h"
#include "PrintMeshAnalysis.h"

#include <QCryptographicHash>
#include <algorithm>
#include <cmath>
#include <limits>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu=.4;
Point column(const std::array<double,12>& t,int c){return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu};}
Point origin(const std::array<double,12>& t){return {t[3]*MmPerLdu,t[11]*MmPerLdu,-t[7]*MmPerLdu};}
Point scaled(Point p,double s){return {p.x*s,p.y*s,p.z*s};}
double dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double length(Point p){return std::sqrt(dot(p,p));}
Point cross(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z};}
Point subtract(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point add(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Point converted(const QVector3D& p){return {double(p.x())*MmPerLdu,double(p.z())*MmPerLdu,-double(p.y())*MmPerLdu};}
double smooth(double x){x=std::clamp(x,0.0,1.0);return x*x*(3.0-2.0*x);}
double contactWeight(double axial,double across,double radial){
    return smooth((std::abs(axial)-3.8)/.9)*smooth((9.6-std::abs(axial))/.5)*
        smooth((3.7-std::abs(across))/.8)*smooth((radial+5.1)/.7)*
        smooth((-2.4-radial)/.5);
}
Point closest(Point p,Point a,Point b,Point c){
    const auto ab=subtract(b,a),ac=subtract(c,a),ap=subtract(p,a);
    const double d1=dot(ab,ap),d2=dot(ac,ap);
    if(d1<=0&&d2<=0)return a;
    const auto bp=subtract(p,b);const double d3=dot(ab,bp),d4=dot(ac,bp);
    if(d3>=0&&d4<=0)return b;
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
    const LDrawGeometry::ReferenceRecord* single=nullptr;
    QVector<const LDrawGeometry::ReferenceRecord*> dual;
    // Certified hinge primitives may be owned by a nested LDraw Part that is
    // composed into the catalog Part. Keep the strict primitive topology,
    // while following source ancestry instead of requiring a root child.
    for(const auto& ref:model.references){
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
    if(dual.size()!=2||dual[0]->parentId!=dual[1]->parentId)return {};
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

bool ClickHingeSemantic::adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
    const PrintMesh& nominal,const FunctionalFeature& hinge,double correction,
    PrintMesh* adjusted,QString* diagnostic){
    if(!adjusted||!source.ok()||!source.sourceModel||nominal.faces.empty()||
       hinge.family!=FunctionalInterfaceFamily::ClickHinge||
       hinge.role!=FunctionalInterfaceRole::Male||
       hinge.constructionRecipe!=QStringLiteral("click-hinge-clh1-paired-arrestors-v1")||
       hinge.evidenceContract!=QStringLiteral("official-ldraw-clh1-clh4-click-lock-v1")||
       hinge.provenance.size()!=1||!std::isfinite(correction)||
       2.186+correction<=0||std::abs(correction)>.15){
        if(diagnostic)*diagnostic=QStringLiteral("The certified click-arrestor adjustment is invalid.");
        return false;
    }
    const auto& model=*source.sourceModel;
    const int owner=hinge.provenance.front().referenceId;
    if(owner<0||owner>=model.references.size()||
       path(model,model.references[owner])!=QStringLiteral("p/clh1.dat"))return false;
    const auto& t=model.references[owner].accumulatedTransform;
    const auto base=origin(t);
    const auto x=scaled(column(t,0),1.0/length(column(t,0)));
    const auto y=scaled(column(t,1),1.0/length(column(t,1)));
    const auto outward=scaled(column(t,2),1.0/length(column(t,2)));
    const auto coordinates=[&](Point p){
        const auto relative=subtract(p,base);
        return std::array<double,3>{dot(relative,x)/MmPerLdu,
            dot(relative,y)/MmPerLdu,dot(relative,outward)/MmPerLdu};
    };
    struct Face{Point a,b,c;};
    QVector<Face> contacts;
    for(const auto& surface:model.surfaces)if(descendant(model,surface.referenceId,owner)){
        if(!surface.certified||surface.triangleIndex<0||surface.triangleIndex>=source.mesh.triangles.size())return false;
        const auto& triangle=source.mesh.triangles[surface.triangleIndex];
        const Face face{converted(triangle.a),converted(triangle.b),converted(triangle.c)};
        bool contact=false;
        for(const auto& p:{face.a,face.b,face.c}){
            const auto c=coordinates(p);
            contact|=contactWeight(c[0],c[1],c[2])>.05;
        }
        if(contact)contacts.push_back(face);
    }
    if(contacts.size()<16){
        if(diagnostic)*diagnostic=QStringLiteral("Both certified arrestor contact surfaces are required.");
        return false;
    }
    *adjusted=nominal;
    if(correction==0.0){
        if(diagnostic)*diagnostic=QStringLiteral("Verified zero click-arrestor adjustment; nominal PreparedMesh retained.");
        return true;
    }
    int moved[2]={0,0};
    for(std::size_t i=0;i<nominal.vertices.size();++i){
        const auto p=nominal.vertices[i];
        const auto c=coordinates(p);
        const double weight=contactWeight(c[0],c[1],c[2]);
        if(weight<=.01)continue;
        double nearest=std::numeric_limits<double>::max();
        for(const auto& face:contacts)
            nearest=std::min(nearest,length(subtract(p,closest(p,face.a,face.b,face.c))));
        if(nearest>.06)continue;
        const double surfaceWeight=weight*std::clamp((.06-nearest)/.04,0.0,1.0);
        if(surfaceWeight<=0)continue;
        adjusted->vertices[i]=add(p,scaled(outward,correction*surfaceWeight));
        ++moved[c[0]<0?0:1];
    }
    if(moved[0]<12||moved[1]<12){
        if(diagnostic)*diagnostic=QStringLiteral("Both arrestors must survive nominal preparation.");
        return false;
    }
    if(!validatePreparedMesh(analyzeSource(*adjusted)).ok()){
        if(diagnostic)*diagnostic=QStringLiteral("Adjusted arrestor contacts failed manifold validation.");
        return false;
    }
    if(diagnostic)*diagnostic=QStringLiteral("Two source-owned click arrestors adjusted by %1 mm (%2 and %3 vertices); bearing and attachment retained.")
        .arg(correction,0,'f',3).arg(moved[0]).arg(moved[1]);
    return true;
}
}

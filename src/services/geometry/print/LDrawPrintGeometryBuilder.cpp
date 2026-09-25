#include "LDrawPrintGeometryBuilder.h"

#include "LDrawPrintPreparationProfile.h"
#include "PrintMeshAnalysis.h"
#include "StudReceivingWallPocketSemantic.h"
#include "StudReceivingAntiStudSemantic.h"
#include "StandardBarSemantic.h"
#include "CClipBarReceiverSemantic.h"
#include "BallJointSemantic.h"

#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <tuple>

namespace PrintGeometry {
namespace {
const LDrawPrintPreparationProfile Profile;
const double WeldMm=Profile.seamWeldMillimetres;
const double PlanarityMm=Profile.planarityToleranceMillimetres;
const double ContactMm=Profile.boundaryContactToleranceMillimetres;
const double InterfaceIntrusionMm=Profile.ordinaryAttachmentIntrusionMillimetres;
const int MaxGroups=int(Profile.maximumSemanticGroups),MaxLoops=int(Profile.maximumBoundaryLoops),
    MaxLoopEdges=int(Profile.maximumLoopEdges),MaxOperands=int(Profile.maximumOperands);
using Edge=std::pair<std::uint32_t,std::uint32_t>;
struct Key{long long x,y,z;bool operator<(const Key&o)const{return std::tie(x,y,z)<std::tie(o.x,o.y,o.z);}};
struct FaceInfo{Face face;int sourceTriangle=-1;};
struct Component{PrintMesh mesh;QVector<int> sourceTriangles;std::vector<std::vector<std::uint32_t>> loops;};
Edge edge(std::uint32_t a,std::uint32_t b){return a<b?Edge{a,b}:Edge{b,a};}
Point convert(const QVector3D&p){return {0.4*double(p.x()),0.4*double(p.z()),-0.4*double(p.y())};}
Key key(const Point&p){return {std::llround(p.x/WeldMm),std::llround(p.y/WeldMm),std::llround(p.z/WeldMm)};}
PrintMesh append(const PrintMesh&a,const PrintMesh&b){PrintMesh o=a;auto offset=std::uint32_t(o.vertices.size());o.vertices.insert(o.vertices.end(),b.vertices.begin(),b.vertices.end());for(auto f:b.faces){for(auto&i:f)i+=offset;o.faces.push_back(f);}return o;}
Point sub(const Point&a,const Point&b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point add(const Point&a,const Point&b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Point cross(const Point&a,const Point&b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(const Point&a,const Point&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double length(const Point&a){return std::sqrt(dot(a,a));}
Point center(const PrintMesh&m,const std::vector<std::uint32_t>&loop){Point c;for(auto i:loop){c.x+=m.vertices[i].x;c.y+=m.vertices[i].y;c.z+=m.vertices[i].z;}const double n=double(loop.size());return {c.x/n,c.y/n,c.z/n};}
bool inBounds(const MeshBounds&b,const Point&p,double e){return b.valid&&p.x>=b.minimum.x-e&&p.x<=b.maximum.x+e&&p.y>=b.minimum.y-e&&p.y<=b.maximum.y+e&&p.z>=b.minimum.z-e&&p.z<=b.maximum.z+e;}
QString roleName(SemanticRole role)
{
    switch(role){
    case SemanticRole::PrimaryBody:return QStringLiteral("primary body");
    case SemanticRole::SubtractivePassage:return QStringLiteral("subtractive passage");
    case SemanticRole::AdditiveAttachment:return QStringLiteral("additive attachment");
    case SemanticRole::HollowAdditiveAttachment:return QStringLiteral("hollow additive attachment");
    }
    return QStringLiteral("semantic");
}

bool planar(const PrintMesh&m,const std::vector<std::uint32_t>&loop)
{
    if(loop.size()<3)return false;Point n;for(std::size_t i=0;i<loop.size();++i){const auto&a=m.vertices[loop[i]],&b=m.vertices[loop[(i+1)%loop.size()]];n.x+=(a.y-b.y)*(a.z+b.z);n.y+=(a.z-b.z)*(a.x+b.x);n.z+=(a.x-b.x)*(a.y+b.y);}const double l=length(n);if(l<1e-12)return false;n={n.x/l,n.y/l,n.z/l};const auto origin=m.vertices[loop.front()];for(auto i:loop)if(std::abs(dot(sub(m.vertices[i],origin),n))>PlanarityMm)return false;return true;
}

std::vector<Component> components(const PrintMesh&all,const std::vector<FaceInfo>&infos);
void normalize(PrintMesh*m);

bool repairCollinearSeam(Component*c,const std::vector<std::uint32_t>&loop)
{
    if(loop.size()!=3)return false;
    int a=0,b=1;double longest=-1.0;
    for(int i=0;i<3;++i)for(int j=i+1;j<3;++j){const double d=length(sub(c->mesh.vertices[loop[i]],c->mesh.vertices[loop[j]]));if(d>longest){longest=d;a=i;b=j;}}
    const int middle=3-a-b;const auto pa=c->mesh.vertices[loop[a]],pb=c->mesh.vertices[loop[b]],pm=c->mesh.vertices[loop[middle]];
    if(longest<=WeldMm||length(cross(sub(pm,pa),sub(pb,pa)))>WeldMm*longest)return false;
    if(std::abs(length(sub(pm,pa))+length(sub(pb,pm))-longest)>WeldMm)return false;
    const auto u=loop[a],v=loop[b],mid=loop[middle];
    for(std::size_t fi=0;fi<c->mesh.faces.size();++fi){const Face f=c->mesh.faces[fi];for(int k=0;k<3;++k){const auto x=f[k],y=f[(k+1)%3],z=f[(k+2)%3];if((x==u&&y==v)||(x==v&&y==u)){c->mesh.faces[fi]={x,mid,z};c->mesh.faces.push_back({mid,y,z});return true;}}}
    return false;
}

void repairCollinearSeams(Component*c)
{
    for(int pass=0;pass<8;++pass){bool changed=false;for(const auto&loop:c->loops)if(repairCollinearSeam(c,loop)){changed=true;break;}if(!changed)return;
        std::vector<FaceInfo>infos(c->mesh.faces.size());for(std::size_t i=0;i<infos.size();++i)infos[i]={c->mesh.faces[i],-1};auto refreshed=components(c->mesh,infos);if(refreshed.size()!=1)return;const auto provenance=c->sourceTriangles;c->mesh=std::move(refreshed[0].mesh);c->loops=std::move(refreshed[0].loops);c->sourceTriangles=provenance;}
}

bool circularPassage(const Component&c,Point*axisOut)
{
    if(c.loops.size()!=2||c.loops[0].size()!=c.loops[1].size()||c.loops[0].size()<4)return false;
    const Point c0=center(c.mesh,c.loops[0]),c1=center(c.mesh,c.loops[1]);Point axis=sub(c1,c0);const double separation=length(axis);if(separation<ContactMm)return false;axis={axis.x/separation,axis.y/separation,axis.z/separation};
    for(int li=0;li<2;++li){const Point origin=li?c1:c0;for(auto id:c.loops[li])if(std::abs(dot(sub(c.mesh.vertices[id],origin),axis))>PlanarityMm)return false;}
    std::vector<double>candidateRadii;for(const auto&p:c.mesh.vertices){const double radius=length(cross(sub(p,c0),axis));if(radius>ContactMm&&std::none_of(candidateRadii.cbegin(),candidateRadii.cend(),[radius](double existing){return std::abs(existing-radius)<=ContactMm;}))candidateRadii.push_back(radius);}
    bool cylindricalWall=false;
    for(double candidateRadius:candidateRadii){Point basis;bool haveBasis=false;double axialMinimum=1e100,axialMaximum=-1e100;int radialPoints=0;bool quadrants[4]={false,false,false,false};
        for(const auto&p:c.mesh.vertices){const Point delta=sub(p,c0);const double axial=dot(delta,axis);const Point radial=sub(delta,{axis.x*axial,axis.y*axial,axis.z*axial});if(std::abs(length(radial)-candidateRadius)>ContactMm)continue;
            if(!haveBasis){basis={radial.x/candidateRadius,radial.y/candidateRadius,radial.z/candidateRadius};haveBasis=true;}
            const Point perpendicular=cross(axis,basis);const double x=dot(radial,basis),y=dot(radial,perpendicular);quadrants[(x<0?2:0)+(y<0?1:0)]=true;axialMinimum=std::min(axialMinimum,axial);axialMaximum=std::max(axialMaximum,axial);++radialPoints;}
        if(radialPoints>=8&&axialMinimum<=ContactMm&&axialMaximum>=separation-ContactMm&&std::all_of(std::begin(quadrants),std::end(quadrants),[](bool value){return value;})){cylindricalWall=true;break;}}
    if(!cylindricalWall)return false;
    *axisOut=axis;return true;
}

struct RoundPassageRecipe {Point origin,axis,profileU,profileV;double radius=0.0,axialExtent=0.0,engagementExtent=0.0;QVector<FunctionalRadialSection>radialProfile;};
bool closeRoundPassage(const Component&c,const Point&axis,PrintMesh*out,int*added,RoundPassageRecipe*recipe)
{
    const Point c0=center(c.mesh,c.loops[0]),c1=center(c.mesh,c.loops[1]);const double separation=length(sub(c1,c0));
    struct Band{double radius=0.0,minimum=1e100,maximum=-1e100;};std::vector<Band>bands;
    for(const auto&p:c.mesh.vertices){const Point delta=sub(p,c0);const double axial=dot(delta,axis),radius=length(cross(delta,axis));if(radius<=ContactMm)continue;auto it=std::find_if(bands.begin(),bands.end(),[radius](const Band&b){return std::abs(b.radius-radius)<=ContactMm;});if(it==bands.end()){bands.push_back({radius,axial,axial});}else{it->minimum=std::min(it->minimum,axial);it->maximum=std::max(it->maximum,axial);}}
    std::sort(bands.begin(),bands.end(),[](const Band&a,const Band&b){return a.radius<b.radius;});if(bands.empty())return false;
    const Band*inner=nullptr,*outer=nullptr;for(const auto&band:bands){if(!inner&&band.minimum>ContactMm&&band.maximum<separation-ContactMm)inner=&band;if(inner&&band.radius>inner->radius+ContactMm&&band.minimum<=ContactMm&&band.maximum>=separation-ContactMm){outer=&band;break;}}
    if(!outer)outer=&bands.front();if(outer->minimum>ContactMm||outer->maximum<separation-ContactMm)return false;
    const Band*basisBand=inner?inner:outer;Point basis;bool found=false;for(const auto&p:c.mesh.vertices){const Point delta=sub(p,c0);const double axial=dot(delta,axis);const Point radial=sub(delta,{axis.x*axial,axis.y*axial,axis.z*axial});if(std::abs(length(radial)-basisBand->radius)<=ContactMm){basis={radial.x/basisBand->radius,radial.y/basisBand->radius,radial.z/basisBand->radius};found=true;break;}}if(!found)return false;const Point perpendicular=cross(axis,basis);
    std::vector<std::pair<double,double>>sections;if(inner&&inner->minimum<inner->maximum)sections={{-InterfaceIntrusionMm,outer->radius},{inner->minimum,outer->radius},{inner->minimum,inner->radius},{inner->maximum,inner->radius},{inner->maximum,outer->radius},{separation+InterfaceIntrusionMm,outer->radius}};else sections={{-InterfaceIntrusionMm,outer->radius},{separation+InterfaceIntrusionMm,outer->radius}};constexpr int Segments=16;PrintMesh mesh;
    for(const auto&section:sections)for(int i=0;i<Segments;++i){const double angle=2.0*3.14159265358979323846*double(i)/double(Segments),cs=std::cos(angle),sn=std::sin(angle);mesh.vertices.push_back({c0.x+axis.x*section.first+(basis.x*cs+perpendicular.x*sn)*section.second,c0.y+axis.y*section.first+(basis.y*cs+perpendicular.y*sn)*section.second,c0.z+axis.z*section.first+(basis.z*cs+perpendicular.z*sn)*section.second});}
    for(std::size_t s=0;s+1<sections.size();++s)for(int i=0;i<Segments;++i){const int n=(i+1)%Segments;const auto a=std::uint32_t(s*Segments+i),b=std::uint32_t(s*Segments+n),d=std::uint32_t((s+1)*Segments+i),e=std::uint32_t((s+1)*Segments+n);mesh.faces.push_back({a,b,d});mesh.faces.push_back({b,e,d});}
    const auto firstCenter=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back({c0.x-axis.x*InterfaceIntrusionMm,c0.y-axis.y*InterfaceIntrusionMm,c0.z-axis.z*InterfaceIntrusionMm});const auto lastCenter=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back({c0.x+axis.x*(separation+InterfaceIntrusionMm),c0.y+axis.y*(separation+InterfaceIntrusionMm),c0.z+axis.z*(separation+InterfaceIntrusionMm)});
    const auto lastBase=std::uint32_t((sections.size()-1)*Segments);for(int i=0;i<Segments;++i){const auto n=(i+1)%Segments;mesh.faces.push_back({firstCenter,std::uint32_t(n),std::uint32_t(i)});mesh.faces.push_back({lastCenter,lastBase+std::uint32_t(i),lastBase+std::uint32_t(n)});}normalize(&mesh);if(!validateBooleanOperand(analyzeSource(mesh)).ok())return false;
    if(recipe){recipe->origin={(c0.x+c1.x)*.5,(c0.y+c1.y)*.5,(c0.z+c1.z)*.5};recipe->axis=axis;recipe->profileU=basis;recipe->profileV=perpendicular;recipe->radius=inner?inner->radius:outer->radius;recipe->axialExtent=separation;recipe->engagementExtent=inner?inner->maximum-inner->minimum:separation;for(const auto&section:sections)recipe->radialProfile.push_back({section.first-separation*.5,section.second});}
    *added=int(mesh.faces.size());*out=std::move(mesh);return true;
}

QString featureIdentity(const QVector<FunctionalFeatureProvenance>&provenance,const RoundPassageRecipe&r)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for(const auto&p:provenance)hash.addData(QString("%1|%2|%3|%4\n").arg(p.sourceFile).arg(p.referenceId).arg(p.sourceLine).arg(p.inverted).toUtf8());
    hash.addData(QString("%1|%2|%3|%4|%5|%6|%7|%8|%9").arg(r.origin.x,0,'g',17).arg(r.origin.y,0,'g',17).arg(r.origin.z,0,'g',17).arg(r.axis.x,0,'g',17).arg(r.axis.y,0,'g',17).arg(r.axis.z,0,'g',17).arg(r.radius,0,'g',17).arg(r.axialExtent,0,'g',17).arg(r.engagementExtent,0,'g',17).toUtf8());for(const auto&s:r.radialProfile)hash.addData(QString("|%1:%2").arg(s.axialPositionMillimetres,0,'g',17).arg(s.radiusMillimetres,0,'g',17).toUtf8());
    return QStringLiteral("round-passage:")+QString::fromLatin1(hash.result().toHex());
}

QVector<int> reviewedPegHoleEvidence(const Component&passage,const LDrawGeometry::LDrawSourceModel&source)
{
    if(passage.loops.size()!=2)return{};const Point openings[2]={center(passage.mesh,passage.loops[0]),center(passage.mesh,passage.loops[1])};QVector<int>matches;
    for(const auto&record:source.references){if(record.fileId<0||record.fileId>=source.files.size()||!source.files[record.fileId].relativePath.endsWith(QStringLiteral("peghole.dat"),Qt::CaseInsensitive))continue;const auto&t=record.accumulatedTransform;const Point origin=convert(QVector3D(float(t[3]),float(t[7]),float(t[11])));for(const auto&opening:openings)if(length(sub(origin,opening))<=ContactMm){matches.push_back(record.id);break;}}
    std::sort(matches.begin(),matches.end());matches.erase(std::unique(matches.begin(),matches.end()),matches.end());return matches.size()==2?matches:QVector<int>{};
}

void appendFeatureProvenance(const Component&component,const LDrawGeometry::LDrawSourceModel&source,QVector<FunctionalFeatureProvenance>*out)
{
    QSet<QString>seen;for(const auto&p:*out)seen.insert(QString("%1|%2|%3|%4").arg(p.sourceFile).arg(p.referenceId).arg(p.sourceLine).arg(p.inverted));
    for(int triangle:component.sourceTriangles){const auto&s=source.surfaces[triangle];const auto&f=source.files[s.fileId];const QString key=QString("%1|%2|%3|%4").arg(f.relativePath).arg(s.referenceId).arg(s.sourceLine).arg(s.inverted);if(seen.contains(key))continue;seen.insert(key);out->push_back({f.relativePath,s.referenceId,s.sourceLine,s.inverted});}
}

struct StandardStudReference { int reference=-1; bool open=false; };
StandardStudReference reviewedStandardStudReference(const Component&component,const LDrawGeometry::LDrawSourceModel&source)
{
    QSet<int> matches;QHash<int,bool> open;
    for(int triangle:component.sourceTriangles){
        if(triangle<0||triangle>=source.surfaces.size())return{};
        int reference=source.surfaces[triangle].referenceId;
        while(reference>=0&&reference<source.references.size()){
            const auto&record=source.references[reference];
            if(record.fileId>=0&&record.fileId<source.files.size()){
                const QString path=source.files[record.fileId].relativePath;
                const bool solid=path.compare(QStringLiteral("p/stud.dat"),Qt::CaseInsensitive)==0||path.compare(QStringLiteral("stud.dat"),Qt::CaseInsensitive)==0;
                const bool hollow=path.compare(QStringLiteral("p/stud2.dat"),Qt::CaseInsensitive)==0||path.compare(QStringLiteral("stud2.dat"),Qt::CaseInsensitive)==0;
                if(solid||hollow){matches.insert(reference);open.insert(reference,hollow);break;}
            }
            reference=record.parentId;
        }
    }
    if(matches.size()!=1)return{};const int reference=*matches.cbegin();return{reference,open.value(reference)};
}

int reviewedReceivingTubeReference(const Component&component,const LDrawGeometry::LDrawSourceModel&source)
{
    QSet<int>matches;
    for(int triangle:component.sourceTriangles){if(triangle<0||triangle>=source.surfaces.size())return -1;int reference=source.surfaces[triangle].referenceId;while(reference>=0&&reference<source.references.size()){const auto&record=source.references[reference];if(record.fileId>=0&&record.fileId<source.files.size()){const QString path=source.files[record.fileId].relativePath;if(path.compare(QStringLiteral("p/stud4.dat"),Qt::CaseInsensitive)==0||path.compare(QStringLiteral("stud4.dat"),Qt::CaseInsensitive)==0){matches.insert(reference);break;}}reference=record.parentId;}}
    return matches.size()==1?*matches.cbegin():-1;
}

Point transformedDirection(const std::array<double,12>&t,double x,double y,double z)
{
    const QVector3D world(float(t[0]*x+t[1]*y+t[2]*z),float(t[4]*x+t[5]*y+t[6]*z),float(t[8]*x+t[9]*y+t[10]*z));
    const Point converted=convert(world);const double magnitude=length(converted);
    return magnitude>1e-12?Point{converted.x/magnitude,converted.y/magnitude,converted.z/magnitude}:Point{};
}

FunctionalFeature standardStudFeature(StandardStudReference stud,const LDrawGeometry::LDrawSourceModel&source,const Component&component)
{
    const int reference=stud.reference;
    FunctionalFeature feature;if(reference<0||reference>=source.references.size())return feature;const auto&record=source.references[reference];const auto&t=record.accumulatedTransform;
    feature.family=FunctionalInterfaceFamily::StandardStud;feature.role=FunctionalInterfaceRole::Male;feature.materialSide=FunctionalMaterialSide::MaterialInside;feature.eligibility=FunctionalEligibility::Eligible;feature.confidence=SemanticConfidence::HighConfidence;feature.operandAction=FunctionalOperandAction::Unite;
    feature.frame.origin=convert(QVector3D(float(t[3]),float(t[7]),float(t[11])));feature.frame.axis=transformedDirection(t,0,-1,0);feature.frame.profileU=transformedDirection(t,1,0,0);feature.frame.profileV=cross(feature.frame.axis,feature.frame.profileU);feature.frame.mirrored=record.mirrored;
    feature.nominalRadiusMillimetres=2.4;feature.nominalDiameterMillimetres=4.8;feature.nominalAxialExtentMillimetres=1.6;feature.nominalEngagementExtentMillimetres=1.6;feature.protectedInnerRadiusMillimetres=stud.open?1.6:0.0;feature.radialProfile={{0.0,2.4},{1.6,2.4}};feature.constructionRecipe=stud.open?QStringLiteral("standard-open-stud-v1"):QStringLiteral("standard-solid-stud-v1");feature.evidenceContract=QStringLiteral("official-ldraw-standard-stud-v1");
    appendFeatureProvenance(component,source,&feature.provenance);if(record.fileId>=0&&record.fileId<source.files.size())feature.provenance.push_back({source.files[record.fileId].relativePath,record.id,record.sourceLine,record.inverted});
    QCryptographicHash hash(QCryptographicHash::Sha256);hash.addData(QString("%1|%2|%3|%4|%5|%6|%7").arg(record.id).arg(record.sourceLine).arg(feature.frame.origin.x,0,'g',17).arg(feature.frame.origin.y,0,'g',17).arg(feature.frame.origin.z,0,'g',17).arg(feature.frame.axis.x,0,'g',17).arg(feature.frame.axis.y,0,'g',17).toUtf8());hash.addData(QString("|%1").arg(feature.frame.axis.z,0,'g',17).toUtf8());feature.stableIdentity=QStringLiteral("standard-stud:")+QString::fromLatin1(hash.result().toHex());feature.governingOperandIdentity=feature.stableIdentity+QStringLiteral(":operand");return feature;
}

bool hasReceivingWallContext(int reference,const LDrawGeometry::LDrawSourceModel&source,const MeshBounds&bodyBounds)
{
    if(reference<0||reference>=source.references.size())return false;const auto&t=source.references[reference].accumulatedTransform;const Point o=convert(QVector3D(float(t[3]),float(t[7]),float(t[11]))),u=transformedDirection(t,1,0,0),v=transformedDirection(t,0,0,1);constexpr double required=3.2;
    const auto inside=[&](const Point&p){return p.x>=bodyBounds.minimum.x-ContactMm&&p.x<=bodyBounds.maximum.x+ContactMm&&p.y>=bodyBounds.minimum.y-ContactMm&&p.y<=bodyBounds.maximum.y+ContactMm&&p.z>=bodyBounds.minimum.z-ContactMm&&p.z<=bodyBounds.maximum.z+ContactMm;};
    return inside({o.x+u.x*required,o.y+u.y*required,o.z+u.z*required})&&inside({o.x-u.x*required,o.y-u.y*required,o.z-u.z*required})&&inside({o.x+v.x*required,o.y+v.y*required,o.z+v.z*required})&&inside({o.x-v.x*required,o.y-v.y*required,o.z-v.z*required});
}

FunctionalFeature receivingTubeFeature(int reference,const LDrawGeometry::LDrawSourceModel&source,const Component&component)
{
    FunctionalFeature f;const auto&record=source.references[reference];const auto&t=record.accumulatedTransform;f.family=FunctionalInterfaceFamily::StudReceivingClutch;f.role=FunctionalInterfaceRole::Female;f.materialSide=FunctionalMaterialSide::MaterialInside;f.eligibility=FunctionalEligibility::Eligible;f.confidence=SemanticConfidence::HighConfidence;f.operandAction=FunctionalOperandAction::Unite;f.frame.origin=convert(QVector3D(float(t[3]),float(t[7]),float(t[11])));f.frame.axis=transformedDirection(t,0,-1,0);f.frame.profileU=transformedDirection(t,1,0,0);f.frame.profileV=cross(f.frame.axis,f.frame.profileU);f.frame.mirrored=record.mirrored;double lo=1e100,hi=-1e100;for(const auto&p:component.mesh.vertices){const double a=dot(sub(p,f.frame.origin),f.frame.axis);lo=std::min(lo,a);hi=std::max(hi,a);}f.frame.origin=add(f.frame.origin,{f.frame.axis.x*lo,f.frame.axis.y*lo,f.frame.axis.z*lo});f.nominalRadiusMillimetres=3.2;f.nominalDiameterMillimetres=6.4;f.nominalAxialExtentMillimetres=hi-lo;f.nominalEngagementExtentMillimetres=hi-lo;f.protectedInnerRadiusMillimetres=2.4;f.radialProfile={{0,3.2},{hi-lo,3.2}};f.constructionRecipe=QStringLiteral("stud-receiving-tube-wall-cell-v1");f.evidenceContract=QStringLiteral("official-ldraw-stud4-tube-wall-cell-v1");appendFeatureProvenance(component,source,&f.provenance);QCryptographicHash hash(QCryptographicHash::Sha256);hash.addData(QString("%1|%2|%3|%4|%5").arg(record.id).arg(record.sourceLine).arg(f.frame.origin.x,0,'g',17).arg(f.frame.origin.y,0,'g',17).arg(f.frame.origin.z,0,'g',17).toUtf8());f.stableIdentity=QStringLiteral("stud-receiving-clutch:")+QString::fromLatin1(hash.result().toHex());f.governingOperandIdentity=f.stableIdentity+QStringLiteral(":operand");return f;
}

bool opensOnOppositeBounds(const Component&passage,const MeshBounds&bounds,const Point&axis)
{
    const Point a=center(passage.mesh,passage.loops[0]),b=center(passage.mesh,passage.loops[1]);
    const double values[3]={std::abs(axis.x),std::abs(axis.y),std::abs(axis.z)};const int dimension=int(std::max_element(values,values+3)-values);
    const auto coordinate=[dimension](const Point&p){return dimension==0?p.x:dimension==1?p.y:p.z;};
    const double minimum=coordinate(bounds.minimum),maximum=coordinate(bounds.maximum),ca=coordinate(a),cb=coordinate(b);
    return (std::abs(ca-minimum)<=ContactMm&&std::abs(cb-maximum)<=ContactMm)||(std::abs(cb-minimum)<=ContactMm&&std::abs(ca-maximum)<=ContactMm);
}

bool hasMatchingOpenings(const Component&shell,const Component&passage)
{
    for(const auto&opening:passage.loops){const Point expected=center(passage.mesh,opening);bool found=false;for(const auto&candidate:shell.loops)if(length(sub(center(shell.mesh,candidate),expected))<=ContactMm){found=true;break;}if(!found)return false;}return true;
}

PrintMesh welded(const LDrawGeometry::PartMesh&source,std::vector<FaceInfo>*infos)
{
    PrintMesh out;std::map<Key,std::uint32_t>ids;
    auto index=[&](const QVector3D&p){Point q=convert(p);auto k=key(q);auto it=ids.find(k);if(it!=ids.end())return it->second;auto id=std::uint32_t(out.vertices.size());ids[k]=id;out.vertices.push_back(q);return id;};
    for(int i=0;i<source.triangles.size();++i){const auto&t=source.triangles[i];Face f{index(t.a),index(t.b),index(t.c)};if(f[0]==f[1]||f[1]==f[2]||f[2]==f[0])continue;out.faces.push_back(f);infos->push_back({f,i});}return out;
}

std::vector<Component> components(const PrintMesh&all,const std::vector<FaceInfo>&infos)
{
    std::map<Edge,std::vector<int>>uses;for(int i=0;i<int(all.faces.size());++i){auto f=all.faces[i];uses[edge(f[0],f[1])].push_back(i);uses[edge(f[1],f[2])].push_back(i);uses[edge(f[2],f[0])].push_back(i);}std::vector<std::vector<int>>adj(all.faces.size());for(const auto&u:uses)for(int a:u.second)for(int b:u.second)if(a!=b)adj[a].push_back(b);
    std::vector<bool>seen(all.faces.size());std::vector<Component>out;
    for(int root=0;root<int(all.faces.size());++root)if(!seen[root]){std::queue<int>q;q.push(root);seen[root]=true;std::vector<int>faces;while(!q.empty()){int n=q.front();q.pop();faces.push_back(n);for(int x:adj[n])if(!seen[x]){seen[x]=true;q.push(x);}}Component c;std::map<std::uint32_t,std::uint32_t>remap;for(int fi:faces){Face f=all.faces[fi];for(auto&v:f){auto it=remap.find(v);if(it==remap.end()){auto id=std::uint32_t(c.mesh.vertices.size());remap[v]=id;c.mesh.vertices.push_back(all.vertices[v]);v=id;}else v=it->second;}c.mesh.faces.push_back(f);c.sourceTriangles.push_back(infos[fi].sourceTriangle);}
        std::map<Edge,int>counts;std::map<std::uint32_t,std::vector<std::uint32_t>>next;for(auto f:c.mesh.faces){std::array<std::pair<std::uint32_t,std::uint32_t>,3>es{{{f[0],f[1]},{f[1],f[2]},{f[2],f[0]}}};for(auto d:es)++counts[edge(d.first,d.second)];}for(auto f:c.mesh.faces){std::array<std::pair<std::uint32_t,std::uint32_t>,3>es{{{f[0],f[1]},{f[1],f[2]},{f[2],f[0]}}};for(auto d:es)if(counts[edge(d.first,d.second)]==1)next[d.first].push_back(d.second);}
        std::set<std::pair<std::uint32_t,std::uint32_t>>used;for(const auto&e:next)for(auto target:e.second)if(!used.count({e.first,target})){std::vector<std::uint32_t>loop;auto start=e.first,at=start,to=target;for(int guard=0;guard<MaxLoopEdges;++guard){used.insert({at,to});loop.push_back(at);at=to;if(at==start)break;auto it=next.find(at);if(it==next.end()||it->second.size()!=1){loop.clear();break;}to=it->second.front();}if(loop.size()>=3)c.loops.push_back(std::move(loop));}
        out.push_back(std::move(c));}
    return out;
}

void normalize(PrintMesh*m){auto a=analyzeSource(*m);if(a.signedVolume<0)for(auto&f:m->faces)std::swap(f[1],f[2]);}
std::vector<std::uint32_t> shiftedLoop(PrintMesh*m,const std::vector<std::uint32_t>&loop,const Point&direction)
{
    std::vector<std::uint32_t> shifted;for(auto id:loop){auto p=m->vertices[id];p.x+=direction.x*InterfaceIntrusionMm;p.y+=direction.y*InterfaceIntrusionMm;p.z+=direction.z*InterfaceIntrusionMm;shifted.push_back(std::uint32_t(m->vertices.size()));m->vertices.push_back(p);}for(std::size_t i=0;i<loop.size();++i){auto n=(i+1)%loop.size();m->faces.push_back({loop[n],loop[i],shifted[i]});m->faces.push_back({loop[n],shifted[i],shifted[n]});}return shifted;
}
bool closeSingle(Component c,PrintMesh*out,int*added,bool allowNonPlanar=false)
{
    if(c.loops.size()!=1||(!allowNonPlanar&&!planar(c.mesh,c.loops[0])))return false;const auto loop=c.loops[0];const Point direction=boundaryAttachmentDirection(c.mesh,loop);if(length(direction)<0.5)return false;const auto shifted=shiftedLoop(&c.mesh,loop,direction);Point mid=center(c.mesh,shifted);auto midId=std::uint32_t(c.mesh.vertices.size());c.mesh.vertices.push_back(mid);for(std::size_t i=0;i<shifted.size();++i)c.mesh.faces.push_back({shifted[(i+1)%shifted.size()],shifted[i],midId});normalize(&c.mesh);if(!validateBooleanOperand(analyzeSource(c.mesh)).ok())return false;*added=int(loop.size()*3);*out=std::move(c.mesh);return true;
}
bool closeAnnularBoundary(Component c,PrintMesh*out,int*added)
{
    if(c.loops.size()!=2||!planar(c.mesh,c.loops[0])||!planar(c.mesh,c.loops[1]))return false;const auto&a=c.loops[0],&b=c.loops[1];if(a.size()!=b.size()||a.size()<3)return false;
    const Point ca=center(c.mesh,a),cb=center(c.mesh,b);if(length(sub(ca,cb))>ContactMm)return false;
    const Point direction=boundaryAttachmentDirection(c.mesh,a);if(length(direction)<0.5)return false;const auto sa=shiftedLoop(&c.mesh,a,direction),sb=shiftedLoop(&c.mesh,b,direction);
    for(int reverse=0;reverse<2;++reverse)for(std::size_t shift=0;shift<sb.size();++shift){PrintMesh candidate=c.mesh;auto bi=[&](std::size_t i){return sb[reverse?(shift+sb.size()-i)%sb.size():(shift+i)%sb.size()];};for(std::size_t i=0;i<sa.size();++i){auto an=sa[i],ax=sa[(i+1)%sa.size()],bn=bi(i),bx=bi(i+1);candidate.faces.push_back({ax,an,bn});candidate.faces.push_back({ax,bn,bx});}normalize(&candidate);if(validateBooleanOperand(analyzeSource(candidate)).ok()){*added=int(a.size()*6);*out=std::move(candidate);return true;}}
    return false;
}
bool contactsBodySurface(const PrintMesh&body,const Point&p,const Point&intoBody)
{
    for(const auto&face:body.faces){
        const auto&a=body.vertices[face[0]],&b=body.vertices[face[1]],&c=body.vertices[face[2]];
        const Point u=sub(b,a),v=sub(c,a),w=sub(p,a);
        const double uu=dot(u,u),uv=dot(u,v),vv=dot(v,v),wu=dot(w,u),wv=dot(w,v),denominator=uu*vv-uv*uv;
        if(denominator<=1e-12)continue;
        const double s=(wu*vv-wv*uv)/denominator,t=(wv*uu-wu*uv)/denominator;
        if(s < -1e-5||t < -1e-5||s+t > 1.0+1e-5)continue;
        const Point q=add(a,add({s*u.x,s*u.y,s*u.z},{t*v.x,t*v.y,t*v.z}));
        if(length(sub(p,q))<=ContactMm&&dot(cross(u,v),intoBody)<-1e-8)return true;
    }
    return false;
}
bool closeConformingAnnularBoundary(Component c,const PrintMesh&body,PrintMesh*out,int*added)
{
    if(c.loops.size()!=2)return false;
    const auto&a=c.loops[0],&b=c.loops[1];
    // Candidate pairing and manifold analysis are bounded; planar annuli retain
    // their existing, independent closure path above.
    if(a.size()!=b.size()||a.size()<8||a.size()>128||
       body.faces.size()*a.size()>250000)return false;
    const Point direction=boundaryAttachmentDirection(c.mesh,a);
    if(length(direction)<0.5)return false;
    for(const auto&loop:c.loops)for(auto id:loop)
        if(!contactsBodySurface(body,c.mesh.vertices[id],direction))return false;
    const auto sa=shiftedLoop(&c.mesh,a,direction),sb=shiftedLoop(&c.mesh,b,direction);
    double shortest=std::numeric_limits<double>::infinity();
    PrintMesh best;
    for(int reverse=0;reverse<2;++reverse)for(std::size_t shift=0;shift<sb.size();++shift){
        auto bi=[&](std::size_t i){return sb[reverse?(shift+sb.size()-i)%sb.size():(shift+i)%sb.size()];};
        double span=0.0;
        bool conforms=true;
        for(std::size_t i=0;i<sa.size();++i){
            const auto&u=c.mesh.vertices[sa[i]],&v=c.mesh.vertices[bi(i)];
            span+=length(sub(u,v));
            const Point middle{(u.x+v.x)*0.5-direction.x*InterfaceIntrusionMm,
                (u.y+v.y)*0.5-direction.y*InterfaceIntrusionMm,
                (u.z+v.z)*0.5-direction.z*InterfaceIntrusionMm};
            conforms&=contactsBodySurface(body,middle,direction);
        }
        if(!conforms)continue;
        if(span>=shortest)continue;
        PrintMesh candidate=c.mesh;
        for(std::size_t i=0;i<sa.size();++i){
            const auto next=(i+1)%sa.size();
            candidate.faces.push_back({sa[next],sa[i],bi(i)});
            candidate.faces.push_back({sa[next],bi(i),bi(next)});
        }
        normalize(&candidate);
        if(!validateBooleanOperand(analyzeSource(candidate)).ok())continue;
        shortest=span;best=std::move(candidate);
    }
    if(!std::isfinite(shortest))return false;
    *added=int(a.size()*6);
    *out=std::move(best);
    return true;
}
bool closePlanarLoops(Component c,PrintMesh*out,int*added)
{
    int count=0;
    for(const auto&loop:c.loops){
        if(!planar(c.mesh,loop))return false;
        const Point mid=center(c.mesh,loop);
        const auto midId=std::uint32_t(c.mesh.vertices.size());
        c.mesh.vertices.push_back(mid);
        for(std::size_t i=0;i<loop.size();++i)c.mesh.faces.push_back({loop[(i+1)%loop.size()],loop[i],midId});
        count+=int(loop.size());
    }
    normalize(&c.mesh);
    *added=count;*out=std::move(c.mesh);return true;
}
bool closeBodyToBallStem(Component c,int bodyLoop,PrintMesh*out,int*added)
{
    if(c.loops.size()!=2||bodyLoop<0||bodyLoop>1||
       !planar(c.mesh,c.loops[0])||!planar(c.mesh,c.loops[1]))return false;
    const auto near=c.loops[bodyLoop],far=c.loops[1-bodyLoop];
    const Point a=center(c.mesh,near),b=center(c.mesh,far);
    const Point outward=sub(a,b);const double extent=length(outward);
    if(near.size()!=far.size()||near.size()<8||extent<ContactMm)return false;
    const Point intoBody{outward.x/extent,outward.y/extent,outward.z/extent};
    const auto shifted=shiftedLoop(&c.mesh,near,intoBody);
    for(const auto&loop:{shifted,far}){
        const auto mid=std::uint32_t(c.mesh.vertices.size());c.mesh.vertices.push_back(center(c.mesh,loop));
        for(std::size_t i=0;i<loop.size();++i)
            c.mesh.faces.push_back({loop[(i+1)%loop.size()],loop[i],mid});
    }
    normalize(&c.mesh);
    if(!validateBooleanOperand(analyzeSource(c.mesh)).ok())return false;
    *added=int(near.size()*3+far.size());*out=std::move(c.mesh);return true;
}
}

LDrawSemanticOperandBuilder::Result LDrawSemanticOperandBuilder::build(const LDrawGeometry::LDrawLoadResult&loaded,const std::function<bool()>&cancellationRequested,bool investigateIntersections)
{
    Result r;if(!loaded.ok()||!loaded.sourceModel){r.diagnostics<<"No hierarchical LDraw source model is available.";return r;}const auto stitched=LDrawCertifiedInterfaceStitcher::stitch(loaded);const auto&effective=stitched.loadResult;r.stitchDiagnostics=stitched.diagnostics;r.diagnostics.append(stitched.diagnostics.messages);
    r.coverage.route=SourceCoverageRoute::SemanticOperands;
    r.coverage.expandedTriangleCount=loaded.mesh.triangles.size();
    r.coverage.stitchedTriangleCount=effective.mesh.triangles.size();
    r.coverage.expandedTriangleForStitchedTriangle=stitched.expandedTriangleForStitchedTriangle;
    r.coverage.groupForStitchedTriangle=QVector<int>(r.coverage.stitchedTriangleCount,-1);
    QElapsedTimer total,phase;total.start();phase.start();std::vector<FaceInfo>infos;r.source=welded(effective.mesh,&infos);r.sourceAnalysis=analyzeSource(r.source);r.sourceConversionAnalysisMs=phase.elapsed();
    r.coverage.degenerateTriangleCount=r.coverage.stitchedTriangleCount-int(infos.size());
    if(!r.sourceAnalysis.finite||!r.sourceAnalysis.indicesValid||r.sourceAnalysis.resourceLimitExceeded){r.status=r.sourceAnalysis.resourceLimitExceeded?Status::ResourceLimitExceeded:Status::OperandValidationFailed;r.diagnostics<<"Source geometry is unsafe for semantic interpretation.";return r;}
    if(cancellationRequested&&cancellationRequested()){r.status=Status::Cancelled;r.diagnostics<<"Semantic preparation was cancelled after Source analysis.";return r;}
    phase.restart();auto groups=components(r.source,infos);r.semanticGroups=int(groups.size());
    for(int i=0;i<int(groups.size());++i){r.coverage.groups.push_back({i,int(groups[i].sourceTriangles.size())});r.coverage.groupedTriangleCount+=int(groups[i].sourceTriangles.size());for(int triangle:groups[i].sourceTriangles)if(triangle>=0&&triangle<r.coverage.groupForStitchedTriangle.size())r.coverage.groupForStitchedTriangle[triangle]=i;}
    r.approximateProvenanceBytes=effective.sourceModel->files.size()*qsizetype(sizeof(LDrawGeometry::SourceFileRecord))+effective.sourceModel->references.size()*qsizetype(sizeof(LDrawGeometry::ReferenceRecord))+effective.sourceModel->surfaces.size()*qsizetype(sizeof(LDrawGeometry::SurfaceRecord));
    if(groups.empty()||groups.size()>MaxGroups){r.status=Status::ResourceLimitExceeded;r.diagnostics<<"Semantic group limit was exceeded.";return r;}int loopCount=0;for(const auto&g:groups)loopCount+=int(g.loops.size());r.sourceBoundaryLoops=loopCount;if(loopCount>MaxLoops){r.status=Status::ResourceLimitExceeded;r.diagnostics<<"Boundary loop limit was exceeded.";return r;}
    auto certified=[&](const Component&g,QStringList*files){QSet<int>ids;for(int ti:g.sourceTriangles){if(ti<0||ti>=effective.sourceModel->surfaces.size())return false;const auto&s=effective.sourceModel->surfaces[ti];if(!s.certified)return false;ids.insert(s.fileId);}for(int id:ids){if(id<0||id>=effective.sourceModel->files.size())return false;const auto&f=effective.sourceModel->files[id];if(f.classification==LDrawGeometry::SourceClassification::Unknown)return false;files->append(f.relativePath);}files->sort();return true;};
    int body=-1;double bodySpan=-1;for(int i=0;i<int(groups.size());++i){if(cancellationRequested&&cancellationRequested()){r.status=Status::Cancelled;r.diagnostics<<"Semantic preparation was cancelled during grouping.";return r;}auto a=analyzeSource(groups[i].mesh);if(!validatePreparedMesh(a,true).ok())continue;auto d=sub(a.bounds.maximum,a.bounds.minimum);double span=d.x*d.y*d.z;if(span>bodySpan){body=i;bodySpan=span;}}
    int passage=-1;FunctionalFeature passageFeature;
    if(body<0){
        for(auto&group:groups)repairCollinearSeams(&group);
        for(int pi=0;pi<int(groups.size())&&body<0;++pi){
            Point axis;const auto passageAnalysis=analyzeSource(groups[pi].mesh);QStringList passageFiles;
            if(passageAnalysis.signedVolume>=0.0||!circularPassage(groups[pi],&axis)||!certified(groups[pi],&passageFiles))continue;
            for(int si=0;si<int(groups.size());++si){if(si==pi||groups[si].loops.size()<2)continue;const auto shellAnalysis=analyzeSource(groups[si].mesh);QStringList shellFiles;
                const auto pegHoleEvidence=reviewedPegHoleEvidence(groups[pi],*effective.sourceModel);if(!opensOnOppositeBounds(groups[pi],shellAnalysis.bounds,axis)||!hasMatchingOpenings(groups[si],groups[pi])||!certified(groups[si],&shellFiles)||pegHoleEvidence.size()!=2)continue;
                PrintMesh closedShell,closedPassage;int shellAdded=0,passageAdded=0;
                RoundPassageRecipe recipe;const bool shellClosed=closePlanarLoops(groups[si],&closedShell,&shellAdded),passageClosed=closeRoundPassage(groups[pi],axis,&closedPassage,&passageAdded,&recipe);
                if(!shellClosed||!passageClosed){r.diagnostics<<QString("Round passage candidate closure rejected (shellClosed=%1, passageClosed=%2).").arg(shellClosed).arg(passageClosed);continue;}
                groups[si].mesh=std::move(closedShell);groups[si].loops.clear();groups[pi].mesh=std::move(closedPassage);groups[pi].loops.clear();
                body=si;passage=pi;r.closureTriangles+=shellAdded+passageAdded;
                passageFeature.family=FunctionalInterfaceFamily::RoundTechnicPassage;passageFeature.role=FunctionalInterfaceRole::Female;passageFeature.materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;passageFeature.eligibility=FunctionalEligibility::Eligible;passageFeature.confidence=SemanticConfidence::HighConfidence;passageFeature.frame={recipe.origin,recipe.axis,recipe.profileU,recipe.profileV,false};passageFeature.nominalRadiusMillimetres=recipe.radius;passageFeature.nominalDiameterMillimetres=recipe.radius*2.0;passageFeature.nominalAxialExtentMillimetres=recipe.axialExtent;passageFeature.nominalEngagementExtentMillimetres=recipe.engagementExtent;passageFeature.radialProfile=recipe.radialProfile;passageFeature.operandAction=FunctionalOperandAction::Subtract;passageFeature.constructionRecipe=QStringLiteral("round-through-passage-v1");passageFeature.evidenceContract=QStringLiteral("official-ldraw-peghole-pair-v1");
                appendFeatureProvenance(groups[pi],*effective.sourceModel,&passageFeature.provenance);appendFeatureProvenance(groups[si],*effective.sourceModel,&passageFeature.provenance);for(int reference:pegHoleEvidence){const auto&record=effective.sourceModel->references[reference];const auto&file=effective.sourceModel->files[record.fileId];passageFeature.provenance.push_back({file.relativePath,record.id,record.sourceLine,record.inverted});}for(const auto&p:passageFeature.provenance)if(p.referenceId>=0&&p.referenceId<effective.sourceModel->references.size())passageFeature.frame.mirrored=passageFeature.frame.mirrored||effective.sourceModel->references[p.referenceId].mirrored;
                std::sort(passageFeature.provenance.begin(),passageFeature.provenance.end(),[](const auto&a,const auto&b){return std::tie(a.sourceFile,a.referenceId,a.sourceLine,a.inverted)<std::tie(b.sourceFile,b.referenceId,b.sourceLine,b.inverted);});passageFeature.stableIdentity=featureIdentity(passageFeature.provenance,recipe);passageFeature.governingOperandIdentity=passageFeature.stableIdentity+QStringLiteral(":operand");
                r.diagnostics<<QString("Certified round through-passage recognized structurally between opposed body openings (passage group %1, body group %2).").arg(pi).arg(si);break;
            }
        }
    }
    if(body<0){
        // Arrangement is diagnostic until a separate, source-proven closure
        // rule can consume every boundary. Its fragments must never be treated
        // as a closed body merely because intersecting surfaces became adjacent.
        // High-operand parts use the existing local composers and must not pay
        // for a second, exploratory all-pairs surface pass.
        constexpr std::size_t MaxDiagnosticOpenGroups=6;
        if(investigateIntersections&&groups.size()<=MaxDiagnosticOpenGroups){
            r.arrangementAttempted=true;
            const auto arranged=LDrawCertifiedInterfaceStitcher::arrangeIntersections(effective);
            r.arrangementBounded=arranged.bounded;
            r.arrangementTransversePairs=arranged.transversePairs;
            r.arrangementCoplanarOverlapPairs=arranged.coplanarOverlapPairs;
            r.arrangementFragments=arranged.fragmentsAfter;
            r.diagnostics<<arranged.diagnostic;
            if(arranged.bounded&&arranged.changed){
                QVector<bool> represented(effective.mesh.triangles.size(),false);
                for(int parent:arranged.stitchedTriangleForFragment)
                    if(parent>=0&&parent<represented.size())represented[parent]=true;
                const bool complete=std::all_of(represented.cbegin(),represented.cend(),[](bool seen){return seen;});
                if(!complete){
                    r.arrangementBounded=false;
                    r.diagnostics<<QStringLiteral("Arranged fragments did not retain complete stitched-source ancestry.");
                }else{
                    std::vector<FaceInfo> arrangedInfos;
                    const auto arrangedMesh=welded(arranged.loadResult.mesh,&arrangedInfos);
                    const auto arrangedGroups=components(arrangedMesh,arrangedInfos);
                    r.arrangementGroups=int(arrangedGroups.size());
                    for(const auto&group:arrangedGroups)for(const auto&loop:group.loops){
                        ++r.arrangementBoundaryLoops;
                        r.arrangementBoundaryLoopEdges.push_back(int(loop.size()));
                    }
                    QStringList sizes;for(int edges:r.arrangementBoundaryLoopEdges)sizes<<QString::number(edges);
                    r.diagnostics<<QString("Arranged source remains diagnostic: groups=%1 boundaryLoops=%2 edges=[%3]; no authored closure was proven.")
                        .arg(r.arrangementGroups).arg(r.arrangementBoundaryLoops).arg(sizes.join(','));
                }
            }
        }else if(investigateIntersections)r.diagnostics<<QStringLiteral("Intersection arrangement diagnostic skipped for high-group source.");
        r.status=Status::OperandValidationFailed;
        r.diagnostics<<"No independently closed body/cavity operand or certified round through-passage body was found.";
        return r;
    }const MeshBounds bodyBounds=analyzeSource(groups[body].mesh).bounds;
    const auto wallPockets=StudReceivingWallPocketSemantic::recognize(effective);
    const auto antiStudBores=StudReceivingAntiStudSemantic::recognize(effective);
    const auto standardBars=StandardBarSemantic::recognize(effective);
    const auto cClips=CClipBarReceiverSemantic::recognize(effective);
    const auto ballJoints=BallJointSemantic::recognize(effective);
    const auto ownedBy=[&](const Component&g,int ancestor){
        if(ancestor<0||g.sourceTriangles.isEmpty())return false;
        for(int triangle:g.sourceTriangles){
            int ref=effective.sourceModel->surfaces[triangle].referenceId;
            while(ref>=0&&ref<effective.sourceModel->references.size()&&ref!=ancestor)
                ref=effective.sourceModel->references[ref].parentId;
            if(ref!=ancestor)return false;
        }
        return true;
    };
    QHash<int,int> ballStemBodyLoop,ballStemHead;
    QSet<int> ballHeadGroups;
    for(const auto& ball:ballJoints){
        if(ball.provenance.size()!=2)continue;
        const int joint=ball.provenance[0].referenceId,sphere=ball.provenance[1].referenceId;
        int head=-1;
        for(int i=0;i<int(groups.size());++i)
            if(i!=body&&groups[i].loops.empty()&&ownedBy(groups[i],sphere)&&
               validateBooleanOperand(analyzeSource(groups[i].mesh)).ok()){
                if(head>=0){head=-1;break;}head=i;
            }
        if(head<0)continue;
        int stem=-1,near=-1;
        for(int i=0;i<int(groups.size());++i){
            const auto&g=groups[i];
            if(i==body||g.loops.size()!=2||!ownedBy(g,joint)||ownedBy(g,sphere)||
               !planar(g.mesh,g.loops[0])||!planar(g.mesh,g.loops[1])||
               g.loops[0].size()!=g.loops[1].size()||g.loops[0].size()<8)continue;
            const Point a=center(g.mesh,g.loops[0]),b=center(g.mesh,g.loops[1]);
            const Point delta=sub(b,a);const double span=length(delta);
            if(span<ContactMm||length(cross(delta,ball.frame.axis))>ContactMm*span)continue;
            const bool contacts[2]={inBounds(bodyBounds,a,ContactMm),inBounds(bodyBounds,b,ContactMm)};
            if(contacts[0]==contacts[1])continue;
            const int candidateNear=contacts[0]?0:1,far=1-candidateNear;
            bool circular=true,embedded=true;
            for(int li=0;li<2;++li){const auto c=center(g.mesh,g.loops[li]);
                for(const auto vertex:g.loops[li]){
                    const auto p=g.mesh.vertices[vertex];const auto radial=sub(p,c);
                    circular&=std::abs(length(radial)-1.6)<=ContactMm&&
                        std::abs(dot(radial,ball.frame.axis))<=PlanarityMm;
                    if(li==far)embedded&=length(sub(p,ball.frame.origin))<ball.nominalRadiusMillimetres-ContactMm;
                }
            }
            if(!circular||!embedded)continue;
            if(stem>=0){stem=-1;break;}
            stem=i;near=candidateNear;
        }
        if(stem>=0){ballStemBodyLoop.insert(stem,near);ballStemHead.insert(stem,head);ballHeadGroups.insert(head);}
    }
    QSet<int> bodyTriangles;
    for(int triangle:groups[body].sourceTriangles)bodyTriangles.insert(triangle);
    QVector<int>order;order<<body;if(passage>=0)order<<passage;for(int loopCount:{1,2})for(int i=0;i<int(groups.size());++i)if(i!=body&&i!=passage&&int(groups[i].loops.size())==loopCount)order<<i;
    for(int i=0;i<int(groups.size());++i)if(ballHeadGroups.contains(i))order<<i;
    for(int index:order){if(cancellationRequested&&cancellationRequested()){r.status=Status::Cancelled;r.diagnostics<<"Semantic preparation was cancelled between operands.";return r;}const auto&g=groups[index];SemanticOperand op;op.sourceMesh=g.mesh;op.confidence=SemanticConfidence::HighConfidence;QStringList files;if(!certified(g,&files)){r.status=Status::UncertifiedGeometry;r.diagnostics<<QString("Group %1 is uncertified or not official.").arg(index);return r;}op.sourceFiles=files;
        if(index==body){op.role=SemanticRole::PrimaryBody;op.feature=SemanticFeature::BodyOrCavity;op.closedMesh=g.mesh;normalize(&op.closedMesh);op.analysis=analyzeSource(op.closedMesh);
            for(const auto& pocket:wallPockets){const int pocketReference=pocket.provenance.front().referenceId;int owned=0,total=0;for(const auto&surface:effective.sourceModel->surfaces){int ref=surface.referenceId;while(ref>=0&&ref<effective.sourceModel->references.size()&&ref!=pocketReference)ref=effective.sourceModel->references[ref].parentId;if(ref==pocketReference){++total;if(bodyTriangles.contains(surface.triangleIndex))++owned;}}if(total>=10&&owned==total){op.functionalFeatures.push_back(pocket);r.diagnostics<<QStringLiteral("Certified WallPocket inner wall and floor surfaces retained in primary body for feature %1.").arg(pocket.stableIdentity);}}
            for(const auto& bore:antiStudBores){const int owner=bore.provenance.front().referenceId;int owned=0,total=0;for(const auto& surface:effective.sourceModel->surfaces){int ref=surface.referenceId;while(ref>=0&&ref<effective.sourceModel->references.size()&&ref!=owner)ref=effective.sourceModel->references[ref].parentId;if(ref==owner){++total;if(bodyTriangles.contains(surface.triangleIndex))++owned;}}if(total>=32&&owned==total){op.functionalFeatures.push_back(bore);r.diagnostics<<QStringLiteral("Certified stud4o AntiStudBore inner wall surfaces retained in primary body for feature %1.").arg(bore.stableIdentity);}}
            for(const auto& bar:standardBars){const int owner=bar.provenance.front().referenceId;int owned=0,total=0;for(const auto& surface:effective.sourceModel->surfaces){int ref=surface.referenceId;while(ref>=0&&ref<effective.sourceModel->references.size()&&ref!=owner)ref=effective.sourceModel->references[ref].parentId;if(ref==owner){++total;if(bodyTriangles.contains(surface.triangleIndex))++owned;}}if(total>=32&&owned==total){op.functionalFeatures.push_back(bar);r.diagnostics<<QStringLiteral("Certified Standard Bar cylindrical surface retained in primary body for feature %1.").arg(bar.stableIdentity);}}
            for(const auto& clip:cClips){const int owner=clip.provenance.front().referenceId;int owned=0,total=0;for(const auto& surface:effective.sourceModel->surfaces){int ref=surface.referenceId;while(ref>=0&&ref<effective.sourceModel->references.size()&&ref!=owner)ref=effective.sourceModel->references[ref].parentId;if(ref==owner){++total;if(bodyTriangles.contains(surface.triangleIndex))++owned;}}if(total>=80&&owned==total){op.functionalFeatures.push_back(clip);r.diagnostics<<QStringLiteral("Certified C-Clip contact arc, throat and compliant arms retained in primary body for feature %1.").arg(clip.stableIdentity);}}
            }
        else if(index==passage){op.role=SemanticRole::SubtractivePassage;op.feature=SemanticFeature::RoundThroughPassage;op.closedMesh=g.mesh;normalize(&op.closedMesh);op.analysis=analyzeSource(op.closedMesh);op.functionalFeatures.push_back(passageFeature);}
        else if(ballHeadGroups.contains(index)){op.role=SemanticRole::AdditiveAttachment;op.feature=SemanticFeature::BodyOrCavity;op.compositionPriority=2;op.closedMesh=g.mesh;normalize(&op.closedMesh);op.analysis=analyzeSource(op.closedMesh);r.diagnostics<<QStringLiteral("Certified closed Ball Joint head retained as an additive operand for group %1.").arg(index);}
        else if(ballStemBodyLoop.contains(index)){op.role=SemanticRole::AdditiveAttachment;op.feature=SemanticFeature::BodyOrCavity;op.compositionPriority=1;if(!closeBodyToBallStem(g,ballStemBodyLoop.value(index),&op.closedMesh,&op.closureTriangles)){r.status=Status::OperandClosureFailed;r.diagnostics<<QStringLiteral("Certified body-to-ball stem closure failed for group %1.").arg(index);return r;}op.analysis=analyzeSource(op.closedMesh);r.diagnostics<<QStringLiteral("Certified Ball Joint stem bridges the body and closed head (groups %1 and %2).").arg(index).arg(ballStemHead.value(index));}
        else {bool contacts=true;for(const auto&loop:g.loops)contacts=contacts&&inBounds(bodyBounds,center(g.mesh,loop),ContactMm);if(!contacts){r.status=Status::AmbiguousBoundary;const auto bounds=analyzeSource(g.mesh).bounds;QStringList centers;for(const auto&loop:g.loops){const auto p=center(g.mesh,loop);centers<<QStringLiteral("(%1,%2,%3)").arg(p.x,0,'g',5).arg(p.y,0,'g',5).arg(p.z,0,'g',5);}r.diagnostics<<QString("Group %1 does not contact the body at its boundary (files: %2; boundary centers: %3; group bounds: [%4,%5,%6]–[%7,%8,%9]; body bounds: [%10,%11,%12]–[%13,%14,%15]).").arg(index).arg(files.join(',')).arg(centers.join(',')).arg(bounds.minimum.x).arg(bounds.minimum.y).arg(bounds.minimum.z).arg(bounds.maximum.x).arg(bounds.maximum.y).arg(bounds.maximum.z).arg(bodyBounds.minimum.x).arg(bodyBounds.minimum.y).arg(bodyBounds.minimum.z).arg(bodyBounds.maximum.x).arg(bodyBounds.maximum.y).arg(bodyBounds.maximum.z);return r;}
            if(g.loops.size()==1){op.role=SemanticRole::AdditiveAttachment;op.feature=SemanticFeature::Stud;if(!closeSingle(g,&op.closedMesh,&op.closureTriangles,passage>=0)){r.status=Status::OperandClosureFailed;r.diagnostics<<QString("Single-loop closure failed for group %1.").arg(index);return r;}const auto studReference=reviewedStandardStudReference(g,*effective.sourceModel);if(studReference.reference>=0){op.functionalFeatures.push_back(standardStudFeature(studReference,*effective.sourceModel,g));r.diagnostics<<QString("Official standard solid stud recognized for group %1.").arg(index);}}
            else if(g.loops.size()==2){op.role=SemanticRole::HollowAdditiveAttachment;op.feature=SemanticFeature::Tube;if(!closeAnnularBoundary(g,&op.closedMesh,&op.closureTriangles)){if(!closeConformingAnnularBoundary(g,r.operands.front().closedMesh,&op.closedMesh,&op.closureTriangles)){r.status=Status::OperandClosureFailed;r.diagnostics<<QString("Nested-loop annular closure failed for group %1.").arg(index);return r;}op.conformingBodyContact=true;}const int receivingReference=reviewedReceivingTubeReference(g,*effective.sourceModel);if(receivingReference>=0&&hasReceivingWallContext(receivingReference,*effective.sourceModel,bodyBounds)){op.functionalFeatures.push_back(receivingTubeFeature(receivingReference,*effective.sourceModel,g));r.diagnostics<<QString("Official stud4 receiving tube recognized in surrounding body-wall context for group %1.").arg(index);}else{const auto studReference=reviewedStandardStudReference(g,*effective.sourceModel);if(studReference.reference>=0&&studReference.open){op.feature=SemanticFeature::Stud;op.functionalFeatures.push_back(standardStudFeature(studReference,*effective.sourceModel,g));r.diagnostics<<QString("Official standard open stud recognized for group %1; its inner bore remains protected.").arg(index);}}}
            else {r.status=Status::UnsupportedBoundaryTopology;r.diagnostics<<QString("Group %1 has %2 boundary loops.").arg(index).arg(g.loops.size());return r;}op.analysis=analyzeSource(op.closedMesh);}
        op.sourceTriangleIndices=g.sourceTriangles;
        QSet<int> operandTriangles;for(int triangle:g.sourceTriangles)operandTriangles.insert(triangle);
        for(const auto& ball:ballJoints){const int owner=ball.provenance.front().referenceId;int owned=0,total=0;for(const auto& surface:effective.sourceModel->surfaces){int ref=surface.referenceId;while(ref>=0&&ref<effective.sourceModel->references.size()&&ref!=owner)ref=effective.sourceModel->references[ref].parentId;if(ref==owner){++total;if(operandTriangles.contains(surface.triangleIndex))++owned;}}if(total>=80&&owned==total){op.functionalFeatures.push_back(ball);r.diagnostics<<QStringLiteral("Certified Ball Joint spherical head retained in one nominal operand for feature %1; production correction is not enabled.").arg(ball.stableIdentity);}}
        const auto operandValidation=validateBooleanOperand(op.analysis);if(!operandValidation.ok()){r.status=Status::OperandValidationFailed;r.diagnostics<<QString("The %1 operand failed Boolean-operand validation: %2 (boundaryEdges=%3, components=%4, nonManifoldVertices=%5, selfIntersections=%6).").arg(roleName(op.role),QString::fromStdString(operandValidation.message)).arg(op.analysis.boundaryEdges).arg(op.analysis.connectedComponents).arg(op.analysis.nonManifoldVertices).arg(op.analysis.selfIntersections);return r;}r.closureTriangles+=op.closureTriangles;r.operands.push_back(std::move(op));r.coverage.groups[index].status=SourceGroupCoverage::SemanticOperand;}
    if(r.operands.size()>MaxOperands){r.status=Status::ResourceLimitExceeded;r.diagnostics<<"Semantic operand limit was exceeded.";return r;}
    if(!r.coverage.complete()){
        QStringList ids;for(int index:r.coverage.uncoveredGroups())ids<<QString::number(index);
        r.status=Status::OperandValidationFailed;
        r.diagnostics<<QStringLiteral("Authoritative source coverage is incomplete: uncovered groups %1 (%2 represented of %3; %4 stitched triangles, %5 grouped, %6 degenerate).").arg(ids.join(',')).arg(r.coverage.representedGroups()).arg(r.coverage.groups.size()).arg(r.coverage.stitchedTriangleCount).arg(r.coverage.groupedTriangleCount).arg(r.coverage.degenerateTriangleCount);
        return r;
    }
    r.semanticGenerationMs=phase.elapsed();r.status=Status::Ready;
    r.diagnostics<<QString("groups=%1 loops=%2 operands=%3 closureTriangles=%4 provenanceBytes~%5 semanticMs=%6 elapsedMs=%7")
        .arg(r.semanticGroups).arg(loopCount).arg(r.operands.size()).arg(r.closureTriangles).arg(r.approximateProvenanceBytes).arg(r.semanticGenerationMs).arg(total.elapsed());return r;
}
} // namespace PrintGeometry

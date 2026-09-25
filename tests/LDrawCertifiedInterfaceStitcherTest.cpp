#include "../src/services/geometry/print/LDrawCertifiedInterfaceStitcher.h"

#include <QCoreApplication>
#include <QTextStream>

#include <algorithm>
#include <cmath>

using namespace LDrawGeometry;
using namespace PrintGeometry;

namespace {
bool check(bool value,const QString& message){if(!value)QTextStream(stderr)<<"FAIL: "<<message<<Qt::endl;return value;}

QVector3D transformed(QVector3D p,bool rotated,bool mirrored)
{
    if(rotated)p=QVector3D(p.z(),p.y(),-p.x());
    if(mirrored)p.setX(-p.x());
    return p;
}

LDrawLoadResult fixture(int splitPoints=1,bool rotated=false,bool mirrored=false,float gap=0.0f,bool certified=true,bool compatibleColor=true)
{
    LDrawLoadResult result;result.sourceModel=std::make_shared<LDrawSourceModel>();
    result.sourceModel->files={{0,QStringLiteral("parts/synthetic.dat"),SourceClassification::Part},{1,QStringLiteral("p/synthetic.dat"),SourceClassification::Primitive}};
    result.sourceModel->references={{0,-1,0,0,{},mirrored,false},{1,0,1,1,{},mirrored,false}};
    const QVector3D a=transformed({0,0,0},rotated,mirrored),b=transformed({10,0,0},rotated,mirrored),c=transformed({0,10,0},rotated,mirrored),d=transformed({0,0,10},rotated,mirrored);
    auto add=[&](QVector3D x,QVector3D y,QVector3D z,int file=0,QString color=QStringLiteral("16"),bool isCertified=true){if(mirrored)std::swap(y,z);Triangle triangle;triangle.a=x;triangle.b=y;triangle.c=z;triangle.normal=QVector3D::crossProduct(y-x,z-x).normalized();triangle.color=color;triangle.backFaceCull=isCertified;const int index=result.mesh.triangles.size();result.mesh.triangles.push_back(triangle);result.sourceModel->surfaces.push_back({index,file,file,index+1,3,isCertified,true,mirrored});};
    add(a,c,b); // The authoritative long edge A-B.
    QVector<QVector3D> chain{a};for(int i=1;i<=splitPoints;++i){const float t=float(i)/float(splitPoints+1);auto point=a+t*(b-a);point.setZ(point.z()+gap);chain.push_back(point);}chain.push_back(b);
    for(int i=0;i+1<chain.size();++i)add(chain[i],chain[i+1],d,1,compatibleColor?QStringLiteral("16"):QStringLiteral("4"),certified);
    add(b,c,d);add(c,a,d);
    result.mesh.hasBounds=true;result.mesh.minimumBounds=QVector3D(std::min({a.x(),b.x(),c.x(),d.x()}),std::min({a.y(),b.y(),c.y(),d.y()}),std::min({a.z(),b.z(),c.z(),d.z()}));result.mesh.maximumBounds=QVector3D(std::max({a.x(),b.x(),c.x(),d.x()}),std::max({a.y(),b.y(),c.y(),d.y()}),std::max({a.z(),b.z(),c.z(),d.z()}));return result;
}

bool samePoint(const QVector3D&a,const QVector3D&b){return a==b;}
bool allCoordinatesAuthoritative(const LDrawLoadResult&before,const LDrawLoadResult&after)
{
    QVector<QVector3D> original;for(const auto&t:before.mesh.triangles)original<<t.a<<t.b<<t.c;
    for(const auto&t:after.mesh.triangles)for(const auto&p:{t.a,t.b,t.c})if(std::none_of(original.cbegin(),original.cend(),[&](const auto&q){return samePoint(p,q);}))return false;return true;
}

LDrawLoadResult ambiguousFixture()
{
    auto result=fixture();const auto original=result.mesh.triangles.takeLast();result.sourceModel->surfaces.removeLast();const QVector3D middle=(original.a+original.b)*0.5f;
    auto add=[&](QVector3D a,QVector3D b,QVector3D c){auto value=original;value.a=a;value.b=b;value.c=c;value.normal=QVector3D::crossProduct(b-a,c-a).normalized();const int index=result.mesh.triangles.size();result.mesh.triangles.push_back(value);result.sourceModel->surfaces.push_back({index,1,1,index+1,3,true,true,false});};
    add(original.a,middle,original.c);add(middle,original.b,original.c);return result;
}

LDrawLoadResult crossingFixture()
{
    LDrawLoadResult result;result.sourceModel=std::make_shared<LDrawSourceModel>();result.sourceModel->files={{0,QStringLiteral("parts/crossing.dat"),SourceClassification::Part}};
    auto add=[&](QVector3D a,QVector3D b,QVector3D c){Triangle value;value.a=a;value.b=b;value.c=c;value.normal=QVector3D::crossProduct(b-a,c-a).normalized();value.color=QStringLiteral("16");value.backFaceCull=true;const int index=result.mesh.triangles.size();result.mesh.triangles.push_back(value);result.sourceModel->surfaces.push_back({index,0,0,index+1,3,true,true,false});};
    add({0,0,0},{10,0,0},{0,10,0});add({5,-5,0},{5,5,0},{10,10,0});return result;
}
}

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);bool ok=true;
    const auto source=fixture();const auto stitched=LDrawCertifiedInterfaceStitcher::stitch(source);
    ok&=check(stitched.changed,"one certified interior vertex is stitched");
    ok&=check(stitched.diagnostics.acceptedSplits==1&&stitched.diagnostics.trianglesBefore==5&&stitched.diagnostics.trianglesAfter==6,"single split metrics");
    ok&=check(stitched.diagnostics.boundariesBefore==3&&stitched.diagnostics.boundariesAfter==0,"single split closes mismatched seam");
    ok&=check(allCoordinatesAuthoritative(source,stitched.loadResult),"no coordinate is invented");
    ok&=check(stitched.loadResult.mesh.minimumBounds==source.mesh.minimumBounds&&stitched.loadResult.mesh.maximumBounds==source.mesh.maximumBounds,"bounds unchanged");
    ok&=check(stitched.loadResult.sourceModel->surfaces.size()==stitched.loadResult.mesh.triangles.size(),"subdivided provenance retained");
    ok&=check(stitched.expandedTriangleForStitchedTriangle.size()==stitched.loadResult.mesh.triangles.size(),"every stitched triangle retains expanded-source ancestry");
    QVector<int> descendants(source.mesh.triangles.size());
    for(int ancestor:stitched.expandedTriangleForStitchedTriangle){
        ok&=check(ancestor>=0&&ancestor<descendants.size(),"stitched ancestor index is valid");
        if(ancestor>=0&&ancestor<descendants.size())++descendants[ancestor];
    }
    for(int count:descendants)ok&=check(count>=1,"no expanded authoritative triangle is lost during stitching");
    for(const auto&s:stitched.loadResult.sourceModel->surfaces)ok&=check(s.certified&&s.triangleIndex>=0,"certification and triangle identity retained");

    const auto multiple=LDrawCertifiedInterfaceStitcher::stitch(fixture(2));
    ok&=check(multiple.changed&&multiple.diagnostics.acceptedSplits==2&&multiple.diagnostics.trianglesAfter==8&&multiple.diagnostics.boundariesAfter==0,"multiple split vertices are ordered and close the seam");
    const auto repeated=LDrawCertifiedInterfaceStitcher::stitch(multiple.loadResult);
    ok&=check(!repeated.changed&&repeated.diagnostics.acceptedSplits==0&&repeated.diagnostics.boundariesBefore==0,"stitching is idempotent");
    const auto deterministic=LDrawCertifiedInterfaceStitcher::stitch(fixture(2));
    ok&=check(deterministic.loadResult.mesh.triangles.size()==multiple.loadResult.mesh.triangles.size(),"stitching output is deterministic");
    for(int i=0;i<multiple.loadResult.mesh.triangles.size();++i){const auto&a=multiple.loadResult.mesh.triangles[i],&b=deterministic.loadResult.mesh.triangles[i];ok&=check(a.a==b.a&&a.b==b.b&&a.c==b.c,"deterministic triangle ordering");}

    for(const auto& transformedFixture:{fixture(1,true,false),fixture(1,false,true)}){const auto value=LDrawCertifiedInterfaceStitcher::stitch(transformedFixture);ok&=check(value.changed&&value.diagnostics.boundariesAfter==0,"rotated/mirrored interface stitched");for(const auto&t:value.loadResult.mesh.triangles)ok&=check(!t.normal.isNull(),"effective winding remains defined");}
    ok&=check(LDrawCertifiedInterfaceStitcher::stitch(fixture(1,false,false,0.00005f)).changed,"collinear match inside established seam tolerance accepted");
    ok&=check(!LDrawCertifiedInterfaceStitcher::stitch(fixture(1,false,false,0.01f)).changed,"spatial gap/outside tolerance rejected");
    ok&=check(!LDrawCertifiedInterfaceStitcher::stitch(fixture(1,false,false,0.0f,false)).changed,"uncertified geometry rejected");
    ok&=check(!LDrawCertifiedInterfaceStitcher::stitch(fixture(1,false,false,0.0f,true,false)).changed,"incompatible material side rejected");
    ok&=check(!LDrawCertifiedInterfaceStitcher::stitch(fixture(0)).changed,"endpoint-only closed topology ignored");
    ok&=check(!LDrawCertifiedInterfaceStitcher::stitch(crossingFixture()).changed,"proper crossing without an authoritative edge vertex is rejected");
    const auto ambiguous=LDrawCertifiedInterfaceStitcher::stitch(ambiguousFixture());ok&=check(!ambiguous.changed&&ambiguous.diagnostics.rejectedAmbiguousCandidates>0,"multiple competing target edges are rejected as ambiguous");
    auto coincident=crossingFixture();
    const auto same=coincident.mesh.triangles.front();
    auto opposed=same;std::swap(opposed.b,opposed.c);opposed.normal=-same.normal;
    coincident.mesh.triangles={same,same,opposed};
    coincident.sourceModel->surfaces.resize(3);
    for(int i=0;i<3;++i)coincident.sourceModel->surfaces[i]={i,0,0,i+1,3,true,true,false};
    SurfaceIntersectionArrangement arranged;arranged.loadResult=coincident;
    arranged.stitchedTriangleForFragment={0,1,2};
    const auto oriented=LDrawCertifiedInterfaceStitcher::classifyOrientedFragments(arranged);
    ok&=check(oriented.bounded&&oriented.sameFacingDuplicates==1&&
              oriented.opposingCoincidentGroups==1&&oriented.fragmentsAfter==2,
              "exact same-facing duplicate consolidates; opposite-facing patch remains unresolved");
    ok&=check(oriented.stitchedTrianglesForFragment.size()==2&&
              oriented.stitchedTrianglesForFragment[0]==QVector<int>({0,1})&&
              oriented.stitchedTrianglesForFragment[1]==QVector<int>({2}),
              "consolidated fragment retains one-to-many stitched ancestry");
    const auto orientedAgain=LDrawCertifiedInterfaceStitcher::classifyOrientedFragments(arranged);
    ok&=check(orientedAgain.stitchedTrianglesForFragment==oriented.stitchedTrianglesForFragment&&
              orientedAgain.loadResult.mesh.triangles.size()==oriented.loadResult.mesh.triangles.size(),
              "oriented classification is deterministic");
    arranged.loadResult.mesh.triangles[1].a.setX(0.0001f);
    const auto offset=LDrawCertifiedInterfaceStitcher::classifyOrientedFragments(arranged);
    ok&=check(offset.sameFacingDuplicates==0&&offset.fragmentsAfter==3,
              "nearby but noncoincident fragment is never removed");
    arranged.loadResult.mesh.triangles[1]=same;
    arranged.loadResult.sourceModel->surfaces[1].certified=false;
    const auto uncertified=LDrawCertifiedInterfaceStitcher::classifyOrientedFragments(arranged);
    ok&=check(uncertified.sameFacingDuplicates==0&&uncertified.fragmentsAfter==3,
              "uncertified coincident fragment is not excluded");
    SurfaceIntersectionArrangement closedArrangement;
    closedArrangement.loadResult=fixture(0);
    for(int i=0;i<closedArrangement.loadResult.mesh.triangles.size();++i)
        closedArrangement.stitchedTriangleForFragment.push_back(i);
    const auto closedOriented=LDrawCertifiedInterfaceStitcher::classifyOrientedFragments(closedArrangement);
    const auto closedCells=LDrawCertifiedInterfaceStitcher::proveMaterialCells(closedOriented);
    ok&=check(closedCells.bounded&&closedCells.candidateCells==1&&closedCells.provenCells==1&&
              closedCells.unresolvedFragments==0&&closedCells.residualBoundaryEdges==0,
              "fully authored oriented tetrahedron proves one closed material cell");
    ok&=check(closedCells.arrangedFragmentStatus.size()==4&&
              std::all_of(closedCells.arrangedFragmentStatus.cbegin(),closedCells.arrangedFragmentStatus.cend(),
                          [](auto status){return status==MaterialCellArrangement::FragmentStatus::ProvenBoundary;}),
              "every authored tetrahedron fragment is accounted for on the proven boundary");
    closedArrangement.loadResult.mesh.triangles.removeLast();
    closedArrangement.loadResult.sourceModel->surfaces.removeLast();
    closedArrangement.stitchedTriangleForFragment.removeLast();
    const auto openCells=LDrawCertifiedInterfaceStitcher::proveMaterialCells(
        LDrawCertifiedInterfaceStitcher::classifyOrientedFragments(closedArrangement));
    ok&=check(openCells.bounded&&openCells.provenCells==0&&openCells.residualBoundaryEdges>0&&
              openCells.unresolvedFragments==openCells.inputFragments,
              "authored opening cannot be inferred as a material-cell wall");
    closedArrangement.loadResult=fixture(0);
    closedArrangement.stitchedTriangleForFragment.clear();
    for(int i=0;i<closedArrangement.loadResult.mesh.triangles.size();++i)
        closedArrangement.stitchedTriangleForFragment.push_back(i);
    auto opposingFace=closedArrangement.loadResult.mesh.triangles.front();
    std::swap(opposingFace.b,opposingFace.c);
    closedArrangement.loadResult.mesh.triangles.push_back(opposingFace);
    auto opposingSurface=closedArrangement.loadResult.sourceModel->surfaces.front();
    opposingSurface.triangleIndex=closedArrangement.loadResult.mesh.triangles.size()-1;
    closedArrangement.loadResult.sourceModel->surfaces.push_back(opposingSurface);
    closedArrangement.stitchedTriangleForFragment.push_back(4);
    const auto partitionCells=LDrawCertifiedInterfaceStitcher::proveMaterialCells(
        LDrawCertifiedInterfaceStitcher::classifyOrientedFragments(closedArrangement));
    ok&=check(partitionCells.provenCells==0&&partitionCells.ambiguousEdges>0,
              "opposing shared patch is not treated as an internal partition without two proven cells");
    const auto duplicateCells=LDrawCertifiedInterfaceStitcher::proveMaterialCells(oriented);
    ok&=check(duplicateCells.bounded&&duplicateCells.explicitDuplicateFragments==1&&
              duplicateCells.arrangedFragmentStatus[1]==MaterialCellArrangement::FragmentStatus::ExplicitDuplicate,
              "same-facing authored duplicate retains explicit arranged-fragment accounting");
    return ok?0:1;
}

#include "../src/services/geometry/print/LocalPrintableOverrideService.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include "../src/services/geometry/print/LDrawPrintPreparationService.h"
#include "../src/services/geometry/print/ManufacturingMeshService.h"
#include "../src/services/geometry/print/LocalPrintableOverrideReview.h"
#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/ThreeMfWriter.h"
#include <lib3mf_implicit.hpp>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>
#include <cstring>
#include <cstdio>

using namespace PrintGeometry;
namespace {
bool fixtureWritesOk=true;
bool check(bool value,const char* message){if(!value)fprintf(stderr,"FAIL: %s\n",message);return value;}
PrintMesh cube()
{
    return {{{0,0,0},{10,0,0},{10,10,0},{0,10,0},{0,0,10},{10,0,10},{10,10,10},{0,10,10}},
        {{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}}};
}
LocalPrintableOverrideService::Context context(const PrintMesh& mesh)
{
    LocalPrintableOverrideService::Context c;c.partId=42;c.partNumber="synthetic";c.source.mesh.ldrawId="synthetic.dat";
    for(const auto& f:mesh.faces){LDrawGeometry::Triangle t;auto convert=[](const Point&p){return QVector3D(float(p.x/0.4),float(-p.z/0.4),float(p.y/0.4));};t.a=convert(mesh.vertices[f[0]]);t.b=convert(mesh.vertices[f[1]]);t.c=convert(mesh.vertices[f[2]]);c.source.mesh.triangles.push_back(t);}
    return c;
}
bool writeMesh(const PrintMesh& mesh,const QString& path)
{
    ThreeMfWriter::Options options;options.modelColor=QColor(160,160,160);QString error;
    const bool result=ThreeMfWriter::write(mesh,path,options,&error);fixtureWritesOk&=result;if(!result)fprintf(stderr,"%s\n",qPrintable(error));return result;
}
QByteArray read(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
bool writeBytes(const QString& path,const QByteArray& data){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(data)==data.size();}
QByteArray stlFixture(const PrintMesh& mesh,bool ascii=false)
{
    if(ascii){
        QByteArray data="solid synthetic\r\n";
        for(const auto& f:mesh.faces){data+=" facet normal 0e0 +0 -0\r\n outer loop\r\n";
            for(auto i:f){const auto& p=mesh.vertices[i];data+=" vertex "+QByteArray::number(p.x,'g',17)+" "+QByteArray::number(p.y,'g',17)+" "+QByteArray::number(p.z,'g',17)+"\r\n";}
            data+=" endloop\r\n endfacet\r\n";}
        return data+"endsolid synthetic\r\n";
    }
    QByteArray data;QDataStream stream(&data,QIODevice::WriteOnly);stream.setByteOrder(QDataStream::LittleEndian);stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    QByteArray header(80,'\0');header.replace(0,5,"solid"); // Binary headers can start with solid.
    stream.writeRawData(header.constData(),header.size());stream<<quint32(mesh.faces.size());
    for(const auto& f:mesh.faces){stream<<float(0)<<float(0)<<float(0);for(auto i:f){const auto& p=mesh.vertices[i];stream<<float(p.x)<<float(p.y)<<float(p.z);}stream<<quint16(0);}
    return data;
}

// Minimal stored ZIP fixture: exercise the actual XML namespace warning without
// a checked-in vendor file, private metadata, or an additional ZIP dependency.
bool vendorFixture(const PrintMesh& mesh,const QString& path,bool badIndex=false)
{
    QByteArray xml="<?xml version=\"1.0\" encoding=\"UTF-8\"?><model unit=\"millimeter\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\"><metadata name=\"Application\">Synthetic repair tool</metadata><metadata name=\"BambuStudio:3mfVersion\">1</metadata><metadata name=\"customXMLNS0:Part\">synthetic</metadata><resources><object id=\"1\" type=\"model\"><mesh><vertices>";
    for(const auto& p:mesh.vertices)xml+="<vertex x=\""+QByteArray::number(p.x)+"\" y=\""+QByteArray::number(p.y)+"\" z=\""+QByteArray::number(p.z)+"\"/>";
    xml+="</vertices><triangles>";
    for(const auto& f:mesh.faces)xml+="<triangle v1=\""+QByteArray::number(badIndex?999999:f[0])+"\" v2=\""+QByteArray::number(f[1])+"\" v3=\""+QByteArray::number(f[2])+"\"/>";
    xml+="</triangles></mesh></object></resources><build><item objectid=\"1\"/></build></model>";
    const QVector<QPair<QByteArray,QByteArray>> entries{
        {"[Content_Types].xml","<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/></Types>"},
        {"_rels/.rels","<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"r0\" Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\" Target=\"/3D/3dmodel.model\"/></Relationships>"},
        {"3D/3dmodel.model",xml}};
    QByteArray zip,central;QDataStream local(&zip,QIODevice::WriteOnly),directory(&central,QIODevice::WriteOnly);
    local.setByteOrder(QDataStream::LittleEndian);directory.setByteOrder(QDataStream::LittleEndian);
    for(const auto& entry:entries){
        quint32 crc=0xffffffff;for(unsigned char byte:entry.second){crc^=byte;for(int bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}crc^=0xffffffff;
        const auto offset=quint32(zip.size()),size=quint32(entry.second.size());const auto nameSize=quint16(entry.first.size());
        local<<quint32(0x04034b50)<<quint16(20)<<quint16(0)<<quint16(0)<<quint16(0)<<quint16(0)<<crc<<size<<size<<nameSize<<quint16(0);
        local.writeRawData(entry.first.constData(),entry.first.size());local.writeRawData(entry.second.constData(),entry.second.size());
        directory<<quint32(0x02014b50)<<quint16(20)<<quint16(20)<<quint16(0)<<quint16(0)<<quint16(0)<<quint16(0)<<crc<<size<<size<<nameSize<<quint16(0)<<quint16(0)<<quint16(0)<<quint16(0)<<quint32(0)<<offset;
        directory.writeRawData(entry.first.constData(),entry.first.size());
    }
    const auto directoryOffset=quint32(zip.size());local.writeRawData(central.constData(),central.size());
    local<<quint32(0x06054b50)<<quint16(0)<<quint16(0)<<quint16(entries.size())<<quint16(entries.size())<<quint32(central.size())<<directoryOffset<<quint16(0);
    QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(zip)==zip.size();
}
}

int main(int argc,char** argv)
{
    // Test-only stalled child proves the parent kills an unresponsive backend.
    if(argc>1&&std::strcmp(argv[1],"--local-override-union-worker")==0&&qEnvironmentVariableIsSet("BRICKSUITE_TEST_STALLED_UNION"))QThread::msleep(20000);
    if(const auto worker=PrintGeometry::LocalPrintableOverrideService::runUnionWorker(argc,argv))return *worker;
    QCoreApplication app(argc,argv);QTemporaryDir temporary;QDir dir(temporary.path());
    LocalPrintableOverrideService service(dir.filePath("overrides"));const auto source=cube();auto c=context(source);
    const auto path=dir.filePath("repair.3mf");QString error;bool ok=true;
    ok&=check(writeMesh(source,path),"write synthetic repair");
    auto accepted=service.importRepaired(c,path);
    if(!accepted.ok())fprintf(stderr,"%s\n",qPrintable(accepted.diagnostic));
    ok&=check(accepted.ok(),"accept exact nominal solid");
    ok&=check(service.load(c).ok(),"reload fingerprinted override and revalidate");
    const auto stored=read(service.storagePath(c));
    const auto vendorPath=dir.filePath("vendor.3mf");
    ok&=check(vendorFixture(source,vendorPath),"write undeclared vendor metadata fixture");
    const auto originalVendor=read(vendorPath),originalSource=LocalPrintableOverrideService::sourceFingerprint(c);
    {
        Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();auto reader=model->QueryReader("3mf");reader->SetStrictModeActive(false);reader->ReadFromFile(vendorPath.toStdString());
        bool found=false;for(Lib3MF_uint32 i=0;i<reader->GetWarningCount();++i){Lib3MF_uint32 code=0;reader->GetWarning(i,code);found|=code==0x80AE;}
        ok&=check(found,"fixture reproduces unresolved metadata namespace warning");
        ok&=check(model->GetMetaDataGroup()->GetMetaDataByKey("","Application")->GetValue()=="Synthetic repair tool","standard Application metadata survives lib3mf parsing");
    }
    ok&=check(service.importRepaired(c,vendorPath).ok()&&service.load(c).ok(),"vendor metadata does not block valid override persistence");
    ok&=check(read(vendorPath)==originalVendor&&LocalPrintableOverrideService::sourceFingerprint(c)==originalSource,"import does not rewrite supplied file or Source");
    auto vendorOpen=source;vendorOpen.faces.pop_back();ok&=check(vendorFixture(vendorOpen,vendorPath),"write vendor open fixture");
    const auto rejectedVendor=service.importRepaired(c,vendorPath);
    ok&=check(!rejectedVendor.ok()&&rejectedVendor.diagnostic.startsWith("Mesh loaded but override validation failed:")&&rejectedVendor.diagnostic.contains("degenerate faces:")&&rejectedVendor.diagnostic.contains("boundary edges: 3")&&read(service.storagePath(c))==stored,"vendor mesh rejection is independent and preserves prior override");
    ok&=check(vendorFixture(source,vendorPath,true),"write malformed geometry fixture");
    const auto malformed=service.importRepaired(c,vendorPath);
    ok&=check(!malformed.ok()&&malformed.diagnostic.startsWith("File/container import failed:")&&read(service.storagePath(c))==stored,"invalid 3MF indices remain container failure and preserve override");
    if(accepted.ok()) {
        ok&=check(accepted.prepared->localRepairedOverride&&accepted.prepared->functionalFeatures.empty(),"override has no invented fit ownership");
        ok&=check(!ManufacturingMeshService().generate(c.source,*accepted.prepared,FitProfile{},PrintOrientation{}).ok(),"service rejects override fit compensation");
        const auto exported=dir.filePath("nominal.3mf");PrintMesh reopened;
        ok&=check(writeMesh(accepted.prepared->mesh,exported)&&LocalPrintableOverrideService::readThreeMf(exported,&reopened,&error)&&validatePreparedMesh(analyzeSource(reopened)).ok(),"normal nominal 3MF export reopens as valid solid");
    }
    auto open=source;open.faces.pop_back();writeMesh(open,path);
    ok&=check(!service.importRepaired(c,path).ok()&&read(service.storagePath(c))==stored,"open import preserves valid override");
    auto nonmanifold=source;nonmanifold.faces.push_back(nonmanifold.faces.front());writeMesh(nonmanifold,path);
    ok&=check(!service.importRepaired(c,path).ok(),"reject duplicate/non-manifold face");
    auto degenerate=source;degenerate.vertices.push_back({5,0,0});degenerate.faces.push_back({0,8,1});
    ok&=check(writeMesh(degenerate,path),"write collinear distinct-index degenerate fixture");
    ok&=check(!service.importRepaired(c,path).ok(),"reject degenerate face");
    auto inverted=source;for(auto& f:inverted.faces)std::swap(f[0],f[1]);writeMesh(inverted,path);
    ok&=check(!service.importRepaired(c,path).ok(),"reject inward orientation");
    auto scaled=source;for(auto& p:scaled.vertices){p.x*=2.5;p.y*=2.5;p.z*=2.5;}writeMesh(scaled,path);
    ok&=check(!service.importRepaired(c,path).ok(),"never normalize a Studio 2.5x import");
    auto dent=source;dent.vertices[6]={5,5,5};writeMesh(dent,path);
    ok&=check(!service.importRepaired(c,path).ok(),"bounds alone cannot admit altered interior surfaces");
    auto two=source;for(auto p:source.vertices){p.x+=20;two.vertices.push_back(p);}for(auto f:source.faces){for(auto& v:f)v+=8;two.faces.push_back(f);}writeMesh(two,path);
    ok&=check(!service.importRepaired(c,path).ok(),"reject multiple solids");
    auto crossing=two;for(std::size_t i=8;i<crossing.vertices.size();++i){crossing.vertices[i].x-=15;crossing.vertices[i].y+=3;crossing.vertices[i].z+=3;}writeMesh(crossing,path);
    ok&=check(analyzeSource(crossing).selfIntersections>0&&!service.importRepaired(c,path).ok(),"reject actual intersecting surfaces");
    auto moved=source;for(auto& p:moved.vertices){p.x+=100;p.y+=80;p.z+=5;}writeMesh(moved,path);
    ok&=check(service.importRepaired(c,path).ok(),"remove slicer translation without changing scale or orientation");
    auto changed=c;changed.source.mesh.triangles[0].a.setX(1);
    ok&=check(service.load(changed).stale&&!service.load(changed).ok(),"changed authoritative geometry is stale");
    changed=c;changed.source.sourceModel=std::make_shared<LDrawGeometry::LDrawSourceModel>();changed.source.sourceModel->files.push_back({0,"changed.dat"});
    ok&=check(service.load(changed).stale,"changed reference ancestry is stale");
    changed=c;changed.partId=43;ok&=check(!service.load(changed).ok(),"internal Part identities are isolated");
    changed=c;changed.source.externalFilePath="external.dat";ok&=check(!service.importRepaired(changed,path).ok(),"ad-hoc files cannot acquire catalog overrides");
    const auto repairSource=dir.filePath("source.3mf");PrintMesh exportedSource;
    auto openContext=context(open);
    ok&=check(LocalPrintableOverrideService::exportRepairSource(openContext,repairSource,&error)&&LocalPrintableOverrideService::readThreeMf(repairSource,&exportedSource,&error),"unprepared open source can be exported and read");
    ok&=check(exportedSource.faces.size()==open.faces.size()&&analyzeSource(exportedSource).bounds.maximum.x==10,"source preserves all triangles in physical mm");
    writeMesh(source,path);
    ok&=check(!service.importRepaired(openContext,path).ok(),"new face distant from authoritative Source fails interior fidelity sampling");
    {
        Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();model->QueryReader("3mf")->ReadFromFile(path.toStdString());model->SetUnit(Lib3MF::eModelUnit::Inch);model->QueryWriter("3mf")->WriteToFile(path.toStdString());
        ok&=check(!service.importRepaired(c,path).ok(),"non-mm units are rejected, never inferred");
    }
    auto document=QJsonDocument::fromJson(read(service.storagePath(c))).object();document["meshFingerprint"]="tampered";
    {QFile file(service.storagePath(c));if(!file.open(QIODevice::WriteOnly))return 1;file.write(QJsonDocument(document).toJson());}
    ok&=check(!service.load(c).ok(),"tampered stored geometry is rejected");
    ok&=check(service.remove(c,&error)&&!service.load(c).ok(),"removal restores no-override state");

    auto overlap=source;for(auto p:source.vertices){p.x+=5;overlap.vertices.push_back(p);}for(auto f:source.faces){for(auto& v:f)v+=8;overlap.faces.push_back(f);}
    auto outer=source;for(auto& p:outer.vertices)p.x*=1.5;
    auto unionContext=context(outer);unionContext.partId=99;
    const auto unionPath=dir.filePath("overlap.3mf");ok&=check(writeMesh(overlap,unionPath),"write two intersecting closed solids");
    const auto unionResult=service.importRepaired(unionContext,unionPath);
    if(!unionResult.ok())fprintf(stderr,"Union test: %s\n",qPrintable(unionResult.diagnostic));
    ok&=check(unionResult.ok()&&unionResult.prepared->localRepairedOverride&&unionResult.prepared->functionalFeatures.empty(),"overlapping valid solids union into nominal override without fit ownership");
    const auto unionStored=read(service.storagePath(unionContext));
    const auto normalization=QJsonDocument::fromJson(unionStored).object()["normalization"].toObject();
    ok&=check(normalization["method"].toString()=="closed-component-union-v1"&&normalization["booleanOperations"].toInt()==1&&service.load(unionContext).ok(),"union provenance persists and normalized mesh reloads through full validation");
    ok&=check(!QJsonDocument::fromJson(stored).object().contains("normalization"),"already-valid single solid records no union");
    ok&=check(writeMesh(two,unionPath),"write separated solids");
    const auto separated=service.importRepaired(unionContext,unionPath);
    ok&=check(!separated.ok()&&separated.diagnostic.contains("merely nearby solids are not unioned")&&read(service.storagePath(unionContext))==unionStored,"separated solids reject before union and preserve override");
    auto nearPair=two;for(std::size_t i=8;i<nearPair.vertices.size();++i)nearPair.vertices[i].x-=9.999999;
    const auto nearby=LocalPrintableOverrideService::normalizeClosedComponents(nearPair);
    ok&=check(!nearby.accepted&&!nearby.normalized,"positive gap is not bridged");
    auto threeSolids=two;for(auto p:source.vertices){p.x+=40;threeSolids.vertices.push_back(p);}for(auto f:source.faces){for(auto& v:f)v+=16;threeSolids.faces.push_back(f);}
    ok&=check(LocalPrintableOverrideService::normalizeClosedComponents(threeSolids).diagnostic.contains("at most two components"),"component count bound rejects a third solid");
    auto invalidPair=overlap;invalidPair.faces.pop_back();ok&=check(writeMesh(invalidPair,unionPath),"write invalid individual component");
    const auto invalidComponent=service.importRepaired(unionContext,unionPath);
    ok&=check(!invalidComponent.ok()&&invalidComponent.diagnostic.contains("Individual component invalid")&&read(service.storagePath(unionContext))==unionStored,"invalid component rejects without union");
    auto changedOuter=outer;changedOuter.vertices[6]={7.5,5,5};auto differentSource=context(changedOuter);differentSource.partId=99;
    ok&=check(writeMesh(overlap,unionPath),"restore overlap fixture");
    const auto fidelityFailure=service.importRepaired(differentSource,unionPath);
    ok&=check(!fidelityFailure.ok()&&fidelityFailure.diagnostic.contains("Closed-component union normalization:")&&fidelityFailure.diagnostic.contains("Source fidelity rejected")&&read(service.storagePath(unionContext))==unionStored,"valid union still fails full Source fidelity and preserves previous override");
    auto excessive=overlap;while(excessive.faces.size()<=2000)excessive.faces.push_back(excessive.faces.front());
    ok&=check(!LocalPrintableOverrideService::normalizeClosedComponents(excessive).accepted,"union triangle workload cap");
    qputenv("BRICKSUITE_TEST_STALLED_UNION","1");
    const auto stalled=LocalPrintableOverrideService::normalizeClosedComponents(overlap);
    qunsetenv("BRICKSUITE_TEST_STALLED_UNION");
    ok&=check(!stalled.accepted&&stalled.diagnostic.contains("terminated")&&stalled.elapsedMilliseconds>=9000&&stalled.elapsedMilliseconds<15000,"stalled Boolean child is terminated at deadline");

    const auto stlPath=dir.filePath("repair.STL");auto stlContext=c;stlContext.partId=501;
    const auto sourceHash=LocalPrintableOverrideService::sourceFingerprint(stlContext);
    for(bool ascii:{false,true}){
        const auto bytes=stlFixture(source,ascii);ok&=check(writeBytes(stlPath,bytes),"write complete STL fixture");
        PrintMesh decoded;ok&=check(LocalPrintableOverrideService::readStl(stlPath,&decoded,&error),"read binary/ASCII STL, including solid binary header");
        ok&=check(decoded.faces.size()==12&&decoded.vertices.size()==8&&analyzeSource(decoded).bounds.maximum.x==10,"STL retains every facet and shares only equal vertices in mm");
        const auto imported=service.importRepaired(stlContext,stlPath);
        ok&=check(imported.ok()&&imported.diagnostic.contains("STL interpreted as millimeters")&&service.load(stlContext).ok(),"valid binary/ASCII STL accepted and persisted in common pipeline");
        ok&=check(read(stlPath)==bytes&&LocalPrintableOverrideService::sourceFingerprint(stlContext)==sourceHash,"STL import does not mutate file or authoritative Source");
    }
    const auto stlStored=read(service.storagePath(stlContext));
    const auto record=QJsonDocument::fromJson(stlStored).object();
    ok&=check(record["importFormat"].toString()=="stl"&&record["interpretedUnits"].toString()=="millimeter","STL millimeter interpretation is recorded");
    for(const auto& invalid:{open,nonmanifold,degenerate,crossing,scaled,dent}){
        ok&=check(writeBytes(stlPath,stlFixture(invalid)),"write invalid STL topology/fidelity fixture");
        const auto rejected=service.importRepaired(stlContext,stlPath);
        ok&=check(!rejected.ok()&&rejected.diagnostic.startsWith("Mesh loaded but override validation failed:")&&read(service.storagePath(stlContext))==stlStored,"invalid STL rejected without replacing stored override");
    }
    ok&=check(writeBytes(stlPath,stlFixture(scaled)),"write STL scale mismatch");
    const auto wrongScale=service.importRepaired(stlContext,stlPath);
    ok&=check(wrongScale.diagnostic.contains("Bounds differ")&&wrongScale.diagnostic.contains("no automatic rescaling")&&wrongScale.diagnostic.contains("STL interpreted as millimeters"),"STL scale mismatch is explicit");
    ok&=check(writeBytes(stlPath,stlFixture(degenerate)),"write STL degenerate facet");
    PrintMesh allFacets;
    ok&=check(LocalPrintableOverrideService::readStl(stlPath,&allFacets,&error)&&allFacets.faces.size()==degenerate.faces.size()&&analyzeSource(allFacets).degenerateFaces==1,"STL degenerate triangles reach validator without being discarded");
    auto repeatedIndex=source;repeatedIndex.faces.push_back({0,0,1});ok&=check(writeBytes(stlPath,stlFixture(repeatedIndex)),"write repeated-vertex STL facet");
    ok&=check(!service.importRepaired(stlContext,stlPath).ok(),"collapsed STL face is not silently removed");
    const auto binary=stlFixture(source),ascii=stlFixture(source,true);
    auto nonfinite=binary;nonfinite.replace(96,4,QByteArray::fromHex("0000807f")); // First vertex x = infinity.
    auto wrongCount=binary;wrongCount.replace(80,4,QByteArray::fromHex("0b000000"));
    const QList<QByteArray> malformedStlFiles{binary.left(binary.size()-1),binary+"trailing",wrongCount,nonfinite,
        ascii.left(ascii.indexOf("endsolid")),QByteArray(ascii).replace(" vertex 0 0 0"," vertex NaN 0 0"),
        QByteArray(ascii).replace(" endloop"," vertex 1 2 3\r\n endloop"),ascii+"facet normal 0 0 0\n"};
    for(const auto& bytes:malformedStlFiles){
        ok&=check(writeBytes(stlPath,bytes),"write malformed STL fixture");
        PrintMesh unchanged=source;
        ok&=check(!LocalPrintableOverrideService::readStl(stlPath,&unchanged,&error)&&unchanged.faces==source.faces,"malformed STL never returns a partial mesh");
        const auto rejected=service.importRepaired(stlContext,stlPath);
        ok&=check(!rejected.ok()&&rejected.diagnostic.startsWith("File/container import failed:")&&read(service.storagePath(stlContext))==stlStored,"malformed STL is a file failure preserving previous override");
    }
    auto nearlyEqual=source;nearlyEqual.vertices.push_back({0.000001,0,0});nearlyEqual.faces.front()[0]=8;
    ok&=check(writeBytes(stlPath,stlFixture(nearlyEqual)),"write near-but-distinct vertices");
    ok&=check(!service.importRepaired(stlContext,stlPath).ok(),"STL does not proximity-weld cracks closed");
    ok&=check(writeBytes(stlPath,stlFixture(overlap)),"write STL overlapping closed solids");
    auto stlUnionContext=unionContext;stlUnionContext.partId=502;
    const auto stlUnion=service.importRepaired(stlUnionContext,stlPath);
    ok&=check(stlUnion.ok()&&QJsonDocument::fromJson(read(service.storagePath(stlUnionContext))).object().contains("normalization"),"STL retains guarded closed-component union and full validation");
    ok&=check(writeBytes(stlPath,stlFixture(source,true))&&service.importRepaired(stlContext,stlPath).ok(),"valid STL replaces a prior override");
    ok&=check(service.remove(stlContext,&error)&&!service.load(stlContext).ok(),"STL override removal restores no-override state");

    // An open internal sheet in a closed envelope has unresolved reverse
    // correspondence. It may be explicitly reviewed, never silently accepted.
    auto composed=source;composed.vertices.insert(composed.vertices.end(),{{4,4,5},{6,4,5},{5,6,5}});composed.faces.push_back({8,9,10});
    auto reviewContext=context(composed);reviewContext.partId=601;
    writeMesh(source,path);
    const auto review=service.importRepaired(reviewContext,path);
    ok&=check(review.reviewable&&!review.ok()&&review.reviewCandidate&&!QFile::exists(service.storagePath(reviewContext)),"reviewable candidate is not persisted or Ready before confirmation");
    ok&=check(!service.importRepaired(reviewContext,path,true).ok(),"confirmation without reviewed fingerprints is refused");
    const auto approved=service.importRepaired(reviewContext,path,true,review.reviewedSourceFingerprint,review.reviewedMeshFingerprint);
    ok&=check(approved.ok()&&approved.prepared->userAcceptedOverride&&approved.prepared->auditStatus()=="user_override_success","explicit hash-bound review creates nominal user override");
    const auto reviewedStored=read(service.storagePath(reviewContext));
    const auto reload=service.load(reviewContext);
    ok&=check(reload.ok()&&reload.prepared->userAcceptedOverride&&reload.prepared->functionalFeatures.empty(),"user acceptance reloads with provenance and no invented fit ownership");
    if(reload.ok()){
        const auto attempt=ManufacturingMeshService().attemptExperimentalOverride(reviewContext.source,*reload.prepared,FitProfile{},PrintOrientation{});
        ok&=check(!attempt.ok()&&attempt.experimentalProvenance["status"]=="experimental_autofit_failed"&&read(service.storagePath(reviewContext))==reviewedStored,"experimental fit safely fails without altering nominal acceptance");
        ok&=check(writeMesh(reload.prepared->mesh,path),"user nominal mesh exports");
        PrintMesh exported;ok&=check(LocalPrintableOverrideService::readThreeMf(path,&exported,&error)&&validatePreparedMesh(analyzeSource(exported)).ok(),"user nominal 3MF reopens valid");
    }
    auto changedReview=reviewContext;changedReview.source.mesh.triangles.last().a.setX(11);
    ok&=check(service.load(changedReview).stale,"source change invalidates user acceptance");
    for(const auto& invalid:{open,nonmanifold,degenerate,crossing,scaled,dent}){
        writeMesh(invalid,path);const auto rejected=service.importRepaired(reviewContext,path);
        ok&=check(!rejected.ok()&&!rejected.reviewable&&read(service.storagePath(reviewContext))==reviewedStored,"topology, bounds, exterior/feature loss cannot be user-approved or replace accepted geometry");
    }
    auto shortened=source;for(auto& p:shortened.vertices)p.x*=.7;writeMesh(shortened,path);
    ok&=check(!service.importRepaired(reviewContext,path).reviewable,"shortened feature is not reviewable");
    // Filled intentional cavity: bounded rays alone must not waive its walls.
    auto cavity=composed;const auto cavityOffset=std::uint32_t(cavity.vertices.size());
    for(auto p:source.vertices)cavity.vertices.push_back({3+p.x*.4,3+p.y*.4,3+p.z*.4});
    for(auto f:source.faces){for(auto& v:f)v+=cavityOffset;std::swap(f[0],f[1]);cavity.faces.push_back(f);}
    writeMesh(source,path);const auto filled=service.importRepaired(context(cavity),path);
    ok&=check(!filled.ok()&&!filled.reviewable,"filled protected void fails even when contained and ray-blocked");
    // Through bore with four walls and two annular rims.
    PrintMesh bore;for(double z:{0.,10.})for(const auto& p:std::vector<Point>{{0,0,z},{10,0,z},{10,10,z},{0,10,z},{3,3,z},{7,3,z},{7,7,z},{3,7,z}})bore.vertices.push_back(p);
    auto quad=[&](unsigned a,unsigned b,unsigned c,unsigned d){bore.faces.push_back({a,b,c});bore.faces.push_back({a,c,d});};
    for(unsigned i=0;i<4;++i){const unsigned j=(i+1)%4;quad(i,j,j+8,i+8);quad(i+4,i+12,j+12,j+4);quad(i+8,j+8,j+12,i+12);quad(i,i+4,j+4,j);}
    ok&=check(validatePreparedMesh(analyzeSource(bore)).ok(),"bore negative fixture is a valid hollow solid");
    const auto filledBore=service.importRepaired(context(bore),path);
    ok&=check(!filledBore.ok()&&!filledBore.reviewable,"filled intentional through bore is not reviewable");
    QString containmentReason;
    ok&=check(!OverrideReview::eligible(reviewContext.source,composed,bore,&containmentReason)&&containmentReason.contains("outside repaired material"),"demonstrated outside source cannot become a reviewable ambiguity");
    writeMesh(open,path);
    ok&=check(service.importRepaired(reviewContext,path,true,review.reviewedSourceFingerprint,review.reviewedMeshFingerprint).stale,"changed file between review and confirmation is stale");
    auto reviewedDocument=QJsonDocument::fromJson(reviewedStored).object();reviewedDocument["acceptanceVersion"]=99;
    writeBytes(service.storagePath(reviewContext),QJsonDocument(reviewedDocument).toJson());
    ok&=check(service.load(reviewContext).stale,"acceptance version change requires new review");
    reviewedDocument=QJsonDocument::fromJson(reviewedStored).object();reviewedDocument["meshFingerprint"]="changed";
    writeBytes(service.storagePath(reviewContext),QJsonDocument(reviewedDocument).toJson());
    ok&=check(service.load(reviewContext).stale,"changed stored repaired mesh fingerprint is stale");
    ok&=check(service.remove(reviewContext,&error)&&!service.load(reviewContext).ok(),"user override removal works");

    // Optional installed-library proof; keeps private files and generated outputs out of fixtures.
    if(argc>=4) {
        const QString library=QString::fromLocal8Bit(argv[1]),reference=QString::fromLocal8Bit(argv[2]),output=QString::fromLocal8Bit(argv[3]);QDir().mkpath(output);
        for(const auto& id:{QStringLiteral("23422"),QStringLiteral("3001")}) {
            auto loaded=LDrawLibraryService::loadPart(library,id);ok&=check(loaded.ok(),"installed proof source loads");
            PrintPreparationRequest request;request.partReference=id;request.ldrawIdentity=id;request.libraryAuthority=library;request.loadResult=loaded;
            const auto native=LDrawPrintPreparationService().prepare(request);
            fprintf(stdout,"Part %s native ready=%d: %s\n",qPrintable(id),native.ready(),qPrintable(native.diagnostic));
            ok&=check(native.ready()==(id=="3001"),"native 23422 failure and 3001 success unchanged");
            if(id=="23422") {
                LocalPrintableOverrideService::Context actual{23422,id,loaded};
                const auto exportPath=QDir(output).filePath("23422-unvalidated-repair-source-mm.3mf");
                ok&=check(LocalPrintableOverrideService::exportRepairSource(actual,exportPath,&error),"export actual 23422 for Ray repair");
                PrintMesh mesh;ok&=check(LocalPrintableOverrideService::readThreeMf(exportPath,&mesh,&error),"reopen actual source export");
                const auto a=analyzeSource(mesh);fprintf(stdout,"23422 source: %zu faces, bounds %.6f x %.6f x %.6f mm\n",mesh.faces.size(),a.bounds.maximum.x-a.bounds.minimum.x,a.bounds.maximum.y-a.bounds.minimum.y,a.bounds.maximum.z-a.bounds.minimum.z);
                PrintMesh referenceMesh;const auto originalFile=read(reference);
                const bool readable=LocalPrintableOverrideService::readThreeMf(reference,&referenceMesh,&error);
                ok&=check(readable,"supplied repaired 3MF container loads");
                if(readable){const auto inspected=analyzeSource(referenceMesh);fprintf(stdout,"Reference mesh: %zu vertices, %zu faces, %zu components, %zu boundary edges, %zu non-manifold edges, %zu non-manifold vertices, %zu degenerates, %zu duplicate faces, %zu intersections\n",referenceMesh.vertices.size(),referenceMesh.faces.size(),inspected.connectedComponents,inspected.boundaryEdges,inspected.nonManifoldEdges,inspected.nonManifoldVertices,inspected.degenerateFaces,inspected.duplicateFaces,inspected.selfIntersections);
                    const auto normalization=LocalPrintableOverrideService::normalizeClosedComponents(referenceMesh);
                    fprintf(stdout,"Component proof: accepted=%d normalized=%d elapsed=%lld ms: %s\n",normalization.accepted,normalization.normalized,static_cast<long long>(normalization.elapsedMilliseconds),qPrintable(normalization.diagnostic));
                }
                const auto imported=service.importRepaired(actual,reference);fprintf(stdout,"Supplied reference accepted=%d: %s\n",imported.ok(),qPrintable(imported.diagnostic));
                ok&=check(!imported.ok()&&imported.diagnostic.startsWith("Mesh loaded but override validation failed:"),"supplied invalid repair fails geometry validation after container load");
                ok&=check(read(reference)==originalFile,"supplied repaired file remains byte-identical");
                if(argc>=5){
                    const auto candidate=QString::fromLocal8Bit(argv[4]);
                    const auto sourceBefore=LocalPrintableOverrideService::sourceFingerprint(actual);
                    const auto reviewed=service.importRepaired(actual,candidate);
                    fprintf(stdout,"Blender reviewable=%d: %s\n",reviewed.reviewable,qPrintable(reviewed.diagnostic));fflush(stdout);
                    ok&=check(reviewed.reviewable&&!reviewed.ok(),"23422 Blender requires review, not automatic acceptance");
                    const auto confirmed=service.importRepaired(actual,candidate,true,reviewed.reviewedSourceFingerprint,reviewed.reviewedMeshFingerprint);
                    const auto restarted=LocalPrintableOverrideService(dir.filePath("overrides")).load(actual);
                    fprintf(stdout,"Blender confirmed=%d: %s\nReload=%d: %s\n",confirmed.ok(),qPrintable(confirmed.diagnostic),restarted.ok(),qPrintable(restarted.diagnostic));fflush(stdout);
                    ok&=check(confirmed.ok()&&restarted.ok()&&restarted.prepared->userAcceptedOverride,"23422 explicit test approval persists across service restart");
                    if(restarted.ok()){
                        ThreeMfWriter::Options options;options.objectName="23422 — User-Accepted Local Override (Nominal)";options.modelColor=QColor(160,160,160);
                        const auto nominal=QDir(output).filePath("23422-user-accepted-nominal.3mf");PrintMesh reopened;
                        ok&=check(ThreeMfWriter::write(restarted.prepared->mesh,nominal,options,&error)&&LocalPrintableOverrideService::readThreeMf(nominal,&reopened,&error)&&validatePreparedMesh(analyzeSource(reopened)).ok(),"23422 user nominal export reopens valid");
                        FitProfile selected;
                        if(argc>=6){FitCalibrationLibrary profiles(QString::fromLocal8Bit(argv[5]));for(const auto& summary:profiles.profiles())if(summary.compatible){profiles.loadProfile(summary.identity,&selected,nullptr);break;}ok&=check(!selected.profileIdentity.isEmpty(),"installed Verified profile remains available independently of feature mapping");}
                        const auto attempt=ManufacturingMeshService().attemptExperimentalOverride(actual.source,*restarted.prepared,selected,PrintOrientation{});
                        fprintf(stdout,"23422 experimental fit: %s\n%s\n",qPrintable(attempt.diagnostic),QJsonDocument(attempt.experimentalProvenance).toJson().constData());
                        ok&=check(!attempt.ok()&&restarted.prepared->userAcceptedOverride,"23422 unmapped fit fails without losing nominal");
                    }
                    ok&=check(sourceBefore==LocalPrintableOverrideService::sourceFingerprint(actual),"23422 source remains unchanged");
                }
            }
        }
    }
    return ok&&fixtureWritesOk?0:1;
}

#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/LDrawObjWriter.h"
#include "../src/services/geometry/BinaryStlWriter.h"
#include "../src/services/geometry/ModelExportPolicy.h"
#include "../src/services/geometry/ThreeMfWriter.h"
#include "../src/services/geometry/PrintOrientation.h"
#include "../src/services/geometry/print/PrintMeshConversion.h"
#include <lib3mf_implicit.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QTemporaryDir>
#include <QTextStream>
#include <QDataStream>
#include <cmath>

namespace {
bool writeFile(const QString& path,const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath()); QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data)==data.size();
}
bool require(bool condition,const char* message){if(!condition)qCritical("%s",message);return condition;}
}

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv); QTemporaryDir temp;
    if(!require(temp.isValid(),"temporary directory"))return 1;
    if(!require(!LDrawLibraryService::validateLibrary(QString()).valid,"unconfigured library rejected"))return 1;
    QTemporaryDir invalid;
    if(!require(!LDrawLibraryService::validateLibrary(invalid.path()).valid,"invalid library rejected"))return 1;
    QDir root(temp.path()); root.mkpath("parts/s"); root.mkpath("p/48");
    if(!writeFile(root.filePath("p/box.dat"),"0 BFC CERTIFY CCW\n3 16 0 0 0 20 0 0 0 20 0\n"))return 1;
    if(!writeFile(root.filePath("parts/3001.dat"),
        "0 BFC CERTIFY CCW\n1 16 0 0 0 1 0 0 0 1 0 0 0 1 box.dat\n"
        "4 4 0 0 0 80 0 0 80 0 40 0 0 40\n2 24 0 0 0 80 0 0\n5 24 0 0 0 80 0 0 0 1 0 80 1 0\n"))return 1;
    QTemporaryDir external;
    const QString externalPath=external.filePath("3001.ldr");
    writeFile(externalPath,"0 External test\n1 16 0 0 0 1 0 0 0 1 0 0 0 1 3001.dat\n");
    const auto externalLoaded=LDrawLibraryService::loadExternalFile(temp.path(),externalPath);
    if(!require(externalLoaded.ok() && externalLoaded.mesh.triangles.size()==3
        && externalLoaded.mesh.ldrawId.isEmpty() && !externalLoaded.externalFilePath.isEmpty()
        && externalLoaded.sourceModel->files.first().relativePath==QStringLiteral("@external/model"),
        "external .ldr root has neutral identity and installed dependencies"))return 1;
    const auto originalHash=externalLoaded.externalContentHash;
    writeFile(externalPath,"0 External test\n1 16 1 0 0 1 0 0 0 1 0 0 0 1 3001.dat\n");
    if(!require(LDrawLibraryService::loadExternalFile(temp.path(),externalPath).externalContentHash!=originalHash,
        "same-size external edit changes content fingerprint"))return 1;
    writeFile(external.filePath("missing.dat"),"1 16 0 0 0 1 0 0 0 1 0 0 0 1 absent.dat\n");
    const auto missingExternal=LDrawLibraryService::loadExternalFile(temp.path(),external.filePath("missing.dat"));
    if(!require(!missingExternal.ok() && missingExternal.error.reference==QStringLiteral("absent.dat"),
        "external missing dependency diagnostic names reference"))return 1;
    writeFile(external.filePath("escape.dat"),"1 16 0 0 0 1 0 0 0 1 0 0 0 1 ../outside.dat\n");
    if(!require(!LDrawLibraryService::loadExternalFile(temp.path(),external.filePath("escape.dat")).ok(),
        "external root does not relax dependency traversal rules"))return 1;
    auto validation=LDrawLibraryService::validateLibrary(temp.path());
    if(!require(validation.valid,"valid library accepted"))return 1;
    auto loaded=LDrawLibraryService::loadPart(temp.path(),"3001");
    if(!require(loaded.externalFilePath.isEmpty() && loaded.mesh.ldrawId==QStringLiteral("3001"),"catalog load after external load retains catalog identity"))return 1;
    if(!require(loaded.ok(),qPrintable(loaded.error.message)))return 1;
    if(!require(loaded.sourceModel && !loaded.sourceModel->files.isEmpty()
                && loaded.sourceModel->files.first().relativePath==QStringLiteral("parts/3001.dat"),
                "production source model identity"))return 1;
    if(!require(!loaded.dependencyFingerprint.dependencies.isEmpty()
                && loaded.dependencyFingerprint.dependencies.first().relativePath==QStringLiteral("parts/3001.dat")
                && loaded.dependencyFingerprint.dependencies.first().size>0,
                "dependency fingerprint records normalized identity and file metadata"))return 1;
    if(!require(loaded.mesh.triangles.size()==3,"triangle and deterministic quad triangulation"))return 1;
    if(!require(loaded.mesh.triangles.at(0).backFaceCull
                && loaded.mesh.triangles.at(1).backFaceCull,
                "certified triangles retain BFC culling metadata"))return 1;
    if(!require(loaded.mesh.hardEdges.size()==1&&loaded.mesh.conditionalEdges.size()==1,"edge preservation"))return 1;
    if(!require(std::abs(loaded.mesh.dimensionsMm().x()-32.0f)<0.01f,"LDU millimetre scale"))return 1;

    writeFile(root.filePath("parts/mpd.dat"),
        "0 FILE main.dat\n1 16 0 0 0 1 0 0 0 1 0 0 0 1 child.dat\n0 FILE child.dat\n3 2 0 0 0 10 0 0 0 10 0\n0 NOFILE\n");
    auto mpd=LDrawLibraryService::loadPart(temp.path(),"mpd");
    if(!require(mpd.ok()&&mpd.mesh.triangles.size()==1,"embedded MPD resolution"))return 1;

    writeFile(root.filePath("parts/MiXeD.dat"),"3 16 0 0 0 1 0 0 0 1 0\n");
    if(!require(LDrawLibraryService::loadPart(temp.path(),"mixed").ok(),"case-insensitive lookup"))return 1;
    writeFile(root.filePath("parts/missing.dat"),"1 16 0 0 0 1 0 0 0 1 0 0 0 1 absent.dat\n");
    if(!require(LDrawLibraryService::loadPart(temp.path(),"missing").error.code==LDrawGeometry::ErrorCode::DependencyMissing,"missing dependency"))return 1;
    writeFile(root.filePath("parts/malformed.dat"),"3 16 nan 0 0 1 0 0 0 1 0\n");
    if(!require(LDrawLibraryService::loadPart(temp.path(),"malformed").error.code==LDrawGeometry::ErrorCode::MalformedSource,"non-finite numeric value"))return 1;

    writeFile(root.filePath("parts/bad.dat"),"1 16 0 0 0 1 0 0 0 1 0 0 0 1 ../secret.dat\n");
    auto traversal=LDrawLibraryService::loadPart(temp.path(),"bad");
    if(!require(traversal.error.code==LDrawGeometry::ErrorCode::TraversalRejected,"traversal rejected"))return 1;
    writeFile(root.filePath("parts/cycle.dat"),"1 16 0 0 0 1 0 0 0 1 0 0 0 1 cycle.dat\n");
    auto cycle=LDrawLibraryService::loadPart(temp.path(),"cycle");
    if(!require(cycle.error.code==LDrawGeometry::ErrorCode::CycleDetected,"cycle detected"))return 1;

    const QString objPath=root.filePath("part.obj"); LDrawGeometry::Error error;
    if(!require(LDrawObjWriter::write(loaded.mesh,objPath,&error),"OBJ export"))return 1;
    QFile obj(objPath); if(!require(obj.open(QIODevice::ReadOnly),"open exported OBJ"))return 1;
    const QByteArray bytes=obj.readAll();
    if(!require(bytes.contains("# Units: millimetres")&&bytes.contains("v 32.000000"),"OBJ units and deterministic conversion"))return 1;

    writeFile(root.filePath("parts/uncertified.dat"),"3 16 0 0 0 10 0 0 0 10 0\n");
    const auto uncertified=LDrawLibraryService::loadPart(temp.path(),"uncertified");
    if(!require(uncertified.ok()&&!uncertified.mesh.triangles.first().backFaceCull,
                "uncertified triangle remains two-sided"))return 1;
    writeFile(root.filePath("parts/noclip.dat"),
        "0 BFC CERTIFY CCW\n0 BFC NOCLIP\n3 16 0 0 0 10 0 0 0 10 0\n");
    const auto noclip=LDrawLibraryService::loadPart(temp.path(),"noclip");
    if(!require(noclip.ok()&&!noclip.mesh.triangles.first().backFaceCull,
                "certified NOCLIP triangle remains two-sided"))return 1;
    writeFile(root.filePath("parts/mirrored.dat"),
        "0 BFC CERTIFY CCW\n1 16 0 0 0 -1 0 0 0 1 0 0 0 1 box.dat\n");
    const auto mirrored=LDrawLibraryService::loadPart(temp.path(),"mirrored");
    if(!require(mirrored.ok()&&mirrored.mesh.triangles.first().backFaceCull,
                "mirrored certified reference retains culling metadata"))return 1;
    writeFile(root.filePath("parts/invert.dat"),
        "0 BFC CERTIFY CCW\n0 BFC INVERTNEXT\n1 16 0 0 0 1 0 0 0 1 0 0 0 1 box.dat\n");
    const auto inverted=LDrawLibraryService::loadPart(temp.path(),"invert");
    if(!require(inverted.ok()&&inverted.mesh.triangles.first().backFaceCull,
                "INVERTNEXT certified reference retains culling metadata"))return 1;

    const auto originalPoint=loaded.mesh.triangles.first().b;
    const QString scaledPath=root.filePath("scaled.obj");
    LDrawObjWriter::Options options;options.uniformScale=2.0;options.partNumber="3001";
    if(!require(LDrawObjWriter::write(loaded.mesh,scaledPath,options,&error),"scaled OBJ export"))return 1;
    QFile scaled(scaledPath);if(!require(scaled.open(QIODevice::ReadOnly),"open scaled OBJ"))return 1;
    const QByteArray scaledBytes=scaled.readAll();
    if(!require(scaledBytes.contains("# Scale: 200.00%")&&scaledBytes.contains("# Dimensions: 64.00 x")
                &&scaledBytes.contains("v 64.000000"),"scaled OBJ coordinates and header"))return 1;
    if(!require(loaded.mesh.triangles.first().b==originalPoint,"scaled export leaves PartMesh unchanged"))return 1;
    const QString fractionalPath=root.filePath("fractional.obj");options.uniformScale=1.005;
    if(!require(LDrawObjWriter::write(loaded.mesh,fractionalPath,options,&error),"fractionally scaled OBJ export"))return 1;
    QFile fractional(fractionalPath);if(!require(fractional.open(QIODevice::ReadOnly),"open fractionally scaled OBJ"))return 1;
    const QByteArray fractionalBytes=fractional.readAll();
    if(!require(fractionalBytes.contains("# Scale: 100.50%")
                &&fractionalBytes.contains("# Dimensions: 32.16 x")
                &&fractionalBytes.contains("v 32.160000"),"100.5 percent scale applied uniformly"))return 1;
    const auto firstNormal=[](const QByteArray& contents){
        for(const QByteArray& line:contents.split('\n'))if(line.startsWith("vn "))return line;
        return QByteArray();
    };
    if(!require(!firstNormal(scaledBytes).isEmpty()
                &&firstNormal(scaledBytes)==firstNormal(fractionalBytes),
                "uniform scale leaves transformed normals unchanged"))return 1;

    PrintGeometry::PrintMesh prepared;
    prepared.vertices={{0,0,0},{10,0,0},{0,20,0}};prepared.faces={{0,1,2}};
    LDrawObjWriter::Options preparedOptions;preparedOptions.geometryLabel="Prepared";preparedOptions.uniformScale=1.005;preparedOptions.partNumber="3001";
    const QString preparedObj=root.filePath("prepared.obj");
    if(!require(LDrawObjWriter::write(prepared,preparedObj,preparedOptions,&error),"prepared OBJ export"))return 1;
    QFile preparedFile(preparedObj);if(!require(preparedFile.open(QIODevice::ReadOnly),"open prepared OBJ"))return 1;const auto preparedBytes=preparedFile.readAll();
    if(!require(preparedBytes.contains("# Geometry: Prepared")&&preparedBytes.contains("v 10.050000 0.000000 0.000000"),"prepared OBJ canonical coordinates and 100.5 percent scale"))return 1;
    const auto preparedBeforeOrientation=prepared;PrintOrientation exportOrientation;exportOrientation.rotate(PrintOrientation::Rotation::XPositive);const auto orientedPrepared=exportOrientation.apply(prepared);
    if(!require(orientedPrepared.vertices[2].x==0&&std::abs(orientedPrepared.vertices[2].y)<0.0001&&orientedPrepared.vertices[2].z==20&&prepared.vertices[2].x==preparedBeforeOrientation.vertices[2].x&&prepared.vertices[2].y==preparedBeforeOrientation.vertices[2].y&&prepared.vertices[2].z==preparedBeforeOrientation.vertices[2].z,"print transform rotates an export copy and leaves nominal PreparedMesh unchanged"))return 1;
    const auto manufacturingBeforeOrientation=prepared;const auto orientedManufacturing=exportOrientation.apply(manufacturingBeforeOrientation);
    if(!require(orientedManufacturing.vertices[2].x==orientedPrepared.vertices[2].x&&orientedManufacturing.vertices[2].y==orientedPrepared.vertices[2].y&&orientedManufacturing.vertices[2].z==orientedPrepared.vertices[2].z&&manufacturingBeforeOrientation.vertices[2].y==prepared.vertices[2].y,"Prepared and ManufacturingMesh exports receive the same print transform"))return 1;
    const QString orientedObjPath=root.filePath("prepared-oriented.obj");preparedOptions.uniformScale=1.0;if(!require(LDrawObjWriter::write(orientedPrepared,orientedObjPath,preparedOptions,&error),"oriented OBJ export"))return 1;QFile orientedObj(orientedObjPath);if(!require(orientedObj.open(QIODevice::ReadOnly)&&orientedObj.readAll().contains("v 0.000000 0.000000 20.000000"),"OBJ receives selected print orientation"))return 1;
    const QString orientedStlPath=root.filePath("prepared-oriented.stl");QString orientedStlError;if(!require(BinaryStlWriter::write(orientedPrepared,orientedStlPath,1.0,&orientedStlError),"oriented STL export"))return 1;QFile orientedStl(orientedStlPath);if(!require(orientedStl.open(QIODevice::ReadOnly),"open oriented STL"))return 1;QDataStream orientedStlStream(orientedStl.readAll().mid(80));orientedStlStream.setByteOrder(QDataStream::LittleEndian);orientedStlStream.setFloatingPointPrecision(QDataStream::SinglePrecision);quint32 orientedCount=0;float onx,ony,onz,oax,oay,oaz,obx,oby,obz,ocx,ocy,ocz;quint16 orientedAttributes=1;orientedStlStream>>orientedCount>>onx>>ony>>onz>>oax>>oay>>oaz>>obx>>oby>>obz>>ocx>>ocy>>ocz>>orientedAttributes;if(!require(orientedCount==1&&std::abs(ocy)<0.0001f&&std::abs(ocz-20.0f)<0.0001f,"STL receives selected print orientation"))return 1;
    exportOrientation.reset();const auto identityPrepared=exportOrientation.apply(prepared);if(!require(identityPrepared.vertices[2].x==prepared.vertices[2].x&&identityPrepared.vertices[2].y==prepared.vertices[2].y&&identityPrepared.vertices[2].z==prepared.vertices[2].z,"identity orientation preserves existing export geometry"))return 1;
    for(double scale:{1.0,1.005,2.0}){
        const QString stlPath=root.filePath(QString("prepared-%1.stl").arg(scale));QString stlError;
        if(!require(BinaryStlWriter::write(prepared,stlPath,scale,&stlError),"binary STL export"))return 1;
        QFile stl(stlPath);if(!require(stl.open(QIODevice::ReadOnly),"open binary STL"))return 1;const QByteArray stlBytes=stl.readAll();
        if(!require(stlBytes.size()==134&&stlBytes.left(21)==QByteArray("BrickSuite binary STL"),"binary STL deterministic header and exact size"))return 1;
        QDataStream stream(stlBytes.mid(80));stream.setByteOrder(QDataStream::LittleEndian);stream.setFloatingPointPrecision(QDataStream::SinglePrecision);quint32 count=0;float nx,ny,nz,ax,ay,az,bx,by,bz,cx,cy,cz;quint16 attributes=1;stream>>count>>nx>>ny>>nz>>ax>>ay>>az>>bx>>by>>bz>>cx>>cy>>cz>>attributes;
        if(!require(count==1&&std::abs(nz-1.0f)<0.0001f&&std::abs(bx-float(10*scale))<0.0001f&&attributes==0,"binary STL count, winding normal, scale and attributes"))return 1;
    }
    QString stlError;if(!require(!BinaryStlWriter::write({},root.filePath("empty.stl"),1.0,&stlError),"empty STL rejected"))return 1;
    const auto ready=ModelExportSelectionPolicy::forStatus(ModelPreparationStatus::Ready,true);
    if(!require(ready.sourceAvailable&&ready.preparedAvailable&&ready.preparedDefault,"Ready export policy"))return 1;
    for(auto state:{ModelPreparationStatus::NotPrepared,ModelPreparationStatus::Unsupported,ModelPreparationStatus::Ambiguous,ModelPreparationStatus::Failed}){const auto p=ModelExportSelectionPolicy::forStatus(state,false);if(!require(p.sourceAvailable&&!p.preparedAvailable&&!p.preparedDefault,"Source fallback export policy"))return 1;}
    ThreeMfWriter::Options threeMfOptions;threeMfOptions.objectName="3001";threeMfOptions.partIdentity="3001";threeMfOptions.modelColor=QColor("#123456");threeMfOptions.uniformScale=1.005;QString threeMfError;const QString threeMfPath=root.filePath("prepared.3mf");
    if(!require(ThreeMfWriter::write(prepared,threeMfPath,threeMfOptions,&threeMfError),"3MF export"))return 1;
    QFile threeMfFile(threeMfPath);if(!require(threeMfFile.open(QIODevice::ReadOnly),"open 3MF package"))return 1;const QByteArray package=threeMfFile.readAll();
    if(!require(package.startsWith("PK")&&!package.contains("C:\\Users\\")&&!package.contains("BrickSuite.db"),"3MF package and metadata privacy"))return 1;
    Lib3MF::CWrapper threeMfWrapper;auto reopened=threeMfWrapper.CreateModel();reopened->QueryReader("3mf")->ReadFromFile(threeMfPath.toStdString());
    if(!require(reopened->GetUnit()==Lib3MF::eModelUnit::MilliMeter,"3MF millimetre unit"))return 1;
    auto meshes=reopened->GetMeshObjects();if(!require(meshes->MoveNext(),"3MF mesh object exists"))return 1;auto reopenedMesh=meshes->GetCurrentMeshObject();
    if(!require(reopenedMesh->GetName()=="3001"&&reopenedMesh->GetVertexCount()==3&&reopenedMesh->GetTriangleCount()==1,"3MF object identity and geometry counts"))return 1;
    const auto scaledVertex=reopenedMesh->GetVertex(1);if(!require(std::abs(scaledVertex.m_Coordinates[0]-10.05f)<0.0001f&&std::abs(scaledVertex.m_Coordinates[1])<0.0001f&&std::abs(scaledVertex.m_Coordinates[2])<0.0001f,"3MF reopened coordinates and scale"))return 1;
    auto buildItems=reopened->GetBuildItems();if(!require(buildItems->MoveNext(),"3MF build item exists"))return 1;Lib3MF_uint32 objectPropertyId=0,objectPropertyIndex=0;reopenedMesh->GetObjectLevelProperty(objectPropertyId,objectPropertyIndex);Lib3MF::sTriangleProperties colorProperty{};reopenedMesh->GetTriangleProperties(0,colorProperty);const auto colorGroup=reopened->GetColorGroupByID(colorProperty.m_ResourceID);const auto defaultColor=colorGroup->GetColor(objectPropertyIndex);const auto exportedColor=colorGroup->GetColor(colorProperty.m_PropertyIDs[0]);if(!require(objectPropertyId==colorProperty.m_ResourceID&&objectPropertyIndex!=colorProperty.m_PropertyIDs[0]&&colorProperty.m_PropertyIDs[0]==colorProperty.m_PropertyIDs[1]&&colorProperty.m_PropertyIDs[1]==colorProperty.m_PropertyIDs[2]&&defaultColor.m_Red==exportedColor.m_Red&&defaultColor.m_Green==exportedColor.m_Green&&defaultColor.m_Blue==exportedColor.m_Blue&&defaultColor.m_Alpha==exportedColor.m_Alpha&&exportedColor.m_Red==0x12&&exportedColor.m_Green==0x34&&exportedColor.m_Blue==0x56&&exportedColor.m_Alpha==255,"3MF selected Model Color"))return 1;
    for(Lib3MF_uint32 i=0;i<reopenedMesh->GetTriangleCount();++i){Lib3MF::sTriangleProperties property{};reopenedMesh->GetTriangleProperties(i,property);const auto triangleColor=reopened->GetColorGroupByID(property.m_ResourceID)->GetColor(property.m_PropertyIDs[0]);if(!require(property.m_PropertyIDs[0]==property.m_PropertyIDs[1]&&property.m_PropertyIDs[1]==property.m_PropertyIDs[2]&&triangleColor.m_Red==0x12&&triangleColor.m_Green==0x34&&triangleColor.m_Blue==0x56&&triangleColor.m_Alpha==255,"every Prepared 3MF triangle uses the selected Model Color"))return 1;}
    auto metadata=reopened->GetMetaDataGroup();if(!require(metadata->GetMetaDataByKey("https://rfstateside.com/bricksuite","Part")->GetValue()=="3001","3MF Part metadata"))return 1;
    const auto sourceMesh=PrintGeometry::PrintMeshConversion::fromPartMesh(loaded.mesh);const QString sourceThreeMfPath=root.filePath("source.3mf");if(!require(ThreeMfWriter::write(sourceMesh,sourceThreeMfPath,threeMfOptions,&threeMfError),"Source 3MF export"))return 1;auto sourceModel=threeMfWrapper.CreateModel();sourceModel->QueryReader("3mf")->ReadFromFile(sourceThreeMfPath.toStdString());auto sourceMeshes=sourceModel->GetMeshObjects();if(!require(sourceMeshes->MoveNext(),"Source 3MF mesh exists"))return 1;auto sourceThreeMfMesh=sourceMeshes->GetCurrentMeshObject();if(!require(sourceThreeMfMesh->GetVertexCount()==sourceMesh.vertices.size()&&sourceThreeMfMesh->GetTriangleCount()==sourceMesh.faces.size(),"Source 3MF geometry counts unchanged"))return 1;for(Lib3MF_uint32 i=0;i<sourceThreeMfMesh->GetTriangleCount();++i){Lib3MF::sTriangleProperties property{};sourceThreeMfMesh->GetTriangleProperties(i,property);const auto triangleColor=sourceModel->GetColorGroupByID(property.m_ResourceID)->GetColor(property.m_PropertyIDs[0]);if(!require(triangleColor.m_Red==0x12&&triangleColor.m_Green==0x34&&triangleColor.m_Blue==0x56&&triangleColor.m_Alpha==255,"Source and Prepared use the same selected Model Color policy"))return 1;}
    threeMfOptions.modelColor=QColor("#ff0000");const QString redPath=root.filePath("prepared-red.3mf");if(!require(ThreeMfWriter::write(prepared,redPath,threeMfOptions,&threeMfError),"alternate Model Color 3MF export"))return 1;auto redModel=threeMfWrapper.CreateModel();redModel->QueryReader("3mf")->ReadFromFile(redPath.toStdString());auto redMeshes=redModel->GetMeshObjects();if(!require(redMeshes->MoveNext(),"alternate-color 3MF mesh exists"))return 1;auto redMesh=redMeshes->GetCurrentMeshObject();Lib3MF::sTriangleProperties redProperty{};redMesh->GetTriangleProperties(0,redProperty);const auto red=redModel->GetColorGroupByID(redProperty.m_ResourceID)->GetColor(redProperty.m_PropertyIDs[0]);if(!require(red.m_Red==255&&red.m_Green==0&&red.m_Blue==0&&red.m_Alpha==255&&redMesh->GetVertexCount()==reopenedMesh->GetVertexCount()&&redMesh->GetTriangleCount()==reopenedMesh->GetTriangleCount()&&std::abs(redMesh->GetVertex(1).m_Coordinates[0]-scaledVertex.m_Coordinates[0])<0.0001f,"changing Model Color leaves 3MF geometry and scale unchanged"))return 1;
    ThreeMfWriter::Options invalidColorOptions=threeMfOptions;invalidColorOptions.modelColor=QColor();if(!require(!ThreeMfWriter::write(prepared,root.filePath("invalid-color.3mf"),invalidColorOptions,&threeMfError),"invalid Model Color rejected instead of silently exporting gray"))return 1;
    const QString orientedThreeMfPath=root.filePath("prepared-oriented.3mf");if(!require(ThreeMfWriter::write(orientedPrepared,orientedThreeMfPath,threeMfOptions,&threeMfError),"oriented 3MF export"))return 1;auto orientedModel=threeMfWrapper.CreateModel();orientedModel->QueryReader("3mf")->ReadFromFile(orientedThreeMfPath.toStdString());auto orientedMeshes=orientedModel->GetMeshObjects();if(!require(orientedMeshes->MoveNext(),"oriented 3MF mesh exists"))return 1;const auto oriented3MfVertex=orientedMeshes->GetCurrentMeshObject()->GetVertex(2);if(!require(std::abs(oriented3MfVertex.m_Coordinates[1])<0.0001f&&std::abs(oriented3MfVertex.m_Coordinates[2]-20.1f)<0.0001f,"3MF receives selected print orientation and preserves Scale"))return 1;
    if(!require(!ThreeMfWriter::write({},root.filePath("empty.3mf"),threeMfOptions,&threeMfError),"empty 3MF rejected"))return 1;
    return 0;
}

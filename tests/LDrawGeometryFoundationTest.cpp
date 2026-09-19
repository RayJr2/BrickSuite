#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/LDrawObjWriter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QTemporaryDir>
#include <QTextStream>
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
    auto validation=LDrawLibraryService::validateLibrary(temp.path());
    if(!require(validation.valid,"valid library accepted"))return 1;
    auto loaded=LDrawLibraryService::loadPart(temp.path(),"3001");
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
    return 0;
}

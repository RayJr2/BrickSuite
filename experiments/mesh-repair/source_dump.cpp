#include "../../src/services/geometry/LDrawLibraryService.h"
#include "../../src/services/geometry/print/PrintMeshConversion.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    if(argc!=4)return 2;
    const auto source=LDrawLibraryService::loadPart(argv[1],argv[2]);
    if(!source.ok()){qCritical("%s",qPrintable(source.error.message));return 1;}
    const QString prefix=QString::fromLocal8Bit(argv[3]);
    QDir().mkpath(QFileInfo(prefix).absolutePath());
    QFile raw(prefix+"-source.off");if(!raw.open(QIODevice::WriteOnly))return 1;
    QTextStream out(&raw);out.setRealNumberPrecision(17);
    out<<"OFF\n"<<source.mesh.triangles.size()*3<<' '<<source.mesh.triangles.size()<<" 0\n";
    for(const auto& t:source.mesh.triangles)for(const auto& p:{t.a,t.b,t.c})out<<0.4*double(p.x())<<' '<<0.4*double(p.z())<<' '<<-0.4*double(p.y())<<'\n';
    for(int i=0;i<source.mesh.triangles.size();++i)out<<"3 "<<3*i<<' '<<3*i+1<<' '<<3*i+2<<'\n';
    out.flush();raw.close();
    const auto candidate=PrintGeometry::PrintMeshConversion::fromPartMesh(source.mesh);
    QFile converted(prefix+"-candidate.off");if(!converted.open(QIODevice::WriteOnly))return 1;
    QTextStream mesh(&converted);mesh.setRealNumberPrecision(17);
    mesh<<"OFF\n"<<candidate.vertices.size()<<' '<<candidate.faces.size()<<" 0\n";
    for(const auto& p:candidate.vertices)mesh<<p.x<<' '<<p.y<<' '<<p.z<<'\n';
    for(const auto& f:candidate.faces)mesh<<"3 "<<f[0]<<' '<<f[1]<<' '<<f[2]<<'\n';
    QJsonArray ancestry;
    for(const auto& surface:source.sourceModel->surfaces){
        QJsonArray references;int r=surface.referenceId;
        while(r>=0){const auto& ref=source.sourceModel->references[r];references.append(QJsonObject{{"reference",r},{"file",source.sourceModel->files[ref.fileId].relativePath},{"line",ref.sourceLine}});r=ref.parentId;}
        ancestry.append(QJsonObject{{"triangle",surface.triangleIndex},{"file",source.sourceModel->files[surface.fileId].relativePath},{"line",surface.sourceLine},{"references",references}});
    }
    QFile provenance(prefix+"-ancestry.json");if(!provenance.open(QIODevice::WriteOnly))return 1;
    provenance.write(QJsonDocument(ancestry).toJson());
    return 0;
}

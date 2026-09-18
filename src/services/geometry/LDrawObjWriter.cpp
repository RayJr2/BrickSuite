#include "LDrawObjWriter.h"
#include <QFile>
#include <QTextStream>

bool LDrawObjWriter::write(const LDrawGeometry::PartMesh& mesh,const QString& path,LDrawGeometry::Error* error)
{
    QFile file(path); if(!file.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Truncate)) {
        if(error) *error={LDrawGeometry::ErrorCode::ExportFailed,"The OBJ file could not be created.",path}; return false;
    }
    QTextStream out(&file); out.setRealNumberNotation(QTextStream::FixedNotation); out.setRealNumberPrecision(6);
    out << "# BrickSuite engineering geometry export\n# LDraw identity: " << mesh.ldrawId.simplified() << "\n";
    out << "# Units: millimetres; axes: X=LDraw X, Y=LDraw Z, Z=-LDraw Y\ng part\n";
    auto writePoint=[&](const QVector3D&p){out<<"v "<<p.x()*.4<<' '<<p.z()*.4<<' '<<-p.y()*.4<<'\n';};
    for(const auto&t:mesh.triangles){writePoint(t.a);writePoint(t.b);writePoint(t.c);}
    for(const auto&t:mesh.triangles){const QVector3D n(t.normal.x(),t.normal.z(),-t.normal.y());out<<"vn "<<n.x()<<' '<<n.y()<<' '<<n.z()<<'\n';}
    for(int i=0;i<mesh.triangles.size();++i){int v=i*3+1,n=i+1;out<<"f "<<v<<"//"<<n<<' '<<v+1<<"//"<<n<<' '<<v+2<<"//"<<n<<'\n';}
    if(out.status()!=QTextStream::Ok){if(error)*error={LDrawGeometry::ErrorCode::ExportFailed,"The OBJ file could not be written completely.",path};return false;}
    return true;
}

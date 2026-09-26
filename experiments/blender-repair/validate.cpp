#include "src/services/geometry/print/LocalPrintableOverrideService.h"
#include "src/services/geometry/print/PrintMeshAnalysis.h"
#include "src/services/geometry/LDrawLibraryService.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <cstdio>
#include <QFile>
#include <QTextStream>
using namespace PrintGeometry;
int main(int argc,char** argv){
    if(auto worker=LocalPrintableOverrideService::runUnionWorker(argc,argv))return *worker;
    QCoreApplication app(argc,argv);QTemporaryDir dir;
    const auto loaded=LDrawLibraryService::loadPart("D:/LDraw","23422");
    if(!loaded.ok())return 2;
    LocalPrintableOverrideService service(dir.path());LocalPrintableOverrideService::Context c{23422,"23422",loaded};
    auto save=[](const PrintMesh& m,const QString& name){QFile file("C:/Programming/Qt/BrickSuite/build/blender-repair-proof/"+name+".off");if(!file.open(QIODevice::WriteOnly))return;QTextStream out(&file);out.setRealNumberPrecision(17);out<<"OFF\n"<<m.vertices.size()<<' '<<m.faces.size()<<" 0\n";for(auto p:m.vertices)out<<p.x<<' '<<p.y<<' '<<p.z<<'\n';for(auto f:m.faces)out<<"3 "<<f[0]<<' '<<f[1]<<' '<<f[2]<<'\n';};
    save(LocalPrintableOverrideService::repairSource(c),"authoritative-source");
    for(int i=1;i<argc;++i){
        const QString path=QString::fromLocal8Bit(argv[i]);PrintMesh mesh;QString error;
        const bool read=QFileInfo(path).suffix()=="stl"?LocalPrintableOverrideService::readStl(path,&mesh,&error):LocalPrintableOverrideService::readThreeMf(path,&mesh,&error);
        printf("FILE %s\n",qPrintable(path));
        if(!read){printf("READ FAILED: %s\n",qPrintable(error));continue;}
        save(mesh,QFileInfo(path).completeBaseName());
        auto a=analyzeSource(mesh);
        printf("vertices=%zu faces=%zu components=%zu boundaries=%zu nonmanifoldEdges=%zu nonmanifoldVertices=%zu intersections=%zu degenerates=%zu\n",mesh.vertices.size(),mesh.faces.size(),a.connectedComponents,a.boundaryEdges,a.nonManifoldEdges,a.nonManifoldVertices,a.selfIntersections,a.degenerateFaces);
        auto result=service.importRepaired(c,path);printf("accepted=%d %s\n",result.ok(),qPrintable(result.diagnostic));fflush(stdout);
    }
}

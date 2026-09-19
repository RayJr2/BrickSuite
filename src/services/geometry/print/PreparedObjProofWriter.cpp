#include "PreparedObjProofWriter.h"
#include <QSaveFile>
#include <QTextStream>
namespace PrintGeometry { bool PreparedObjProofWriter::write(const PrintMesh&m,const QString&path,QString*error){QSaveFile f(path);if(!f.open(QIODevice::WriteOnly|QIODevice::Text)){if(error)*error=f.errorString();return false;}QTextStream s(&f);s.setRealNumberNotation(QTextStream::FixedNotation);s.setRealNumberPrecision(9);s<<"# BrickSuite developer Prepared OBJ\n# Z-up millimetres; canonical nominal 100% scale\n";for(auto&p:m.vertices)s<<"v "<<p.x<<' '<<p.y<<' '<<p.z<<'\n';for(auto&t:m.faces)s<<"f "<<t[0]+1<<' '<<t[1]+1<<' '<<t[2]+1<<'\n';if(!f.commit()){if(error)*error=f.errorString();return false;}return true;} }

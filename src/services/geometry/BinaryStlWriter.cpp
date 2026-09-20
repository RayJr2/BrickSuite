#include "BinaryStlWriter.h"
#include <QDataStream>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
bool BinaryStlWriter::write(const PrintGeometry::PrintMesh&m,const QString&path,double scale,QString*error){
 if(!(scale>0)||!std::isfinite(scale)||m.faces.empty()||m.vertices.empty()||m.faces.size()>UINT32_MAX){if(error)*error="The selected geometry is invalid or empty.";return false;}QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)){if(error)*error="The STL file could not be created.";return false;}QByteArray h(80,'\0');const QByteArray t="BrickSuite binary STL";std::copy(t.begin(),t.end(),h.begin());if(f.write(h)!=80)return false;QDataStream s(&f);s.setByteOrder(QDataStream::LittleEndian);s.setFloatingPointPrecision(QDataStream::SinglePrecision);s<<quint32(m.faces.size());
 for(const auto&x:m.faces){if(x[0]>=m.vertices.size()||x[1]>=m.vertices.size()||x[2]>=m.vertices.size()){if(error)*error="The selected geometry contains an invalid face.";return false;}auto a=m.vertices[x[0]],b=m.vertices[x[1]],c=m.vertices[x[2]];double ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z,vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;double nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx,l=std::sqrt(nx*nx+ny*ny+nz*nz);if(!(l>0)){if(error)*error="The selected geometry contains a degenerate face.";return false;}s<<float(nx/l)<<float(ny/l)<<float(nz/l);for(auto p:{a,b,c})s<<float(p.x*scale)<<float(p.y*scale)<<float(p.z*scale);s<<quint16(0);}if(s.status()!=QDataStream::Ok||!f.commit()){if(error)*error="The STL file could not be written completely.";return false;}return true;}

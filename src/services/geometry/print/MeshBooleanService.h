#pragma once
#include "PrintMesh.h"
#include <QString>
namespace PrintGeometry {
class MeshBooleanService {public:virtual ~MeshBooleanService()=default;virtual MeshBooleanResult unite(const PrintMesh&,const PrintMesh&)=0;virtual MeshBooleanResult subtract(const PrintMesh&,const PrintMesh&)=0;virtual QString versionIdentity()const=0;};
}

#pragma once
#include "MeshBooleanService.h"
namespace PrintGeometry {
class McutMeshBooleanService final : public MeshBooleanService {
public:
    MeshBooleanResult unite(const PrintMesh& source,const PrintMesh& additive) override;
    MeshBooleanResult subtract(const PrintMesh& source,const PrintMesh& passage) override;
    QString versionIdentity() const override;
    static bool available();
    static const char* version();
};
}

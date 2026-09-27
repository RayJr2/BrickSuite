#pragma once
#include "MeshBooleanService.h"
namespace PrintGeometry {
class McutMeshBooleanService final : public MeshBooleanService {
public:
    // Preserve trusted calibration generators; catalog preparation explicitly
    // selects Isolated. Never fall back from Isolated to InProcess on failure.
    enum class Execution { InProcess, Isolated };
    explicit McutMeshBooleanService(Execution execution=Execution::InProcess)
        : m_isolated(execution==Execution::Isolated) {}
    // Optional executable/deadline seam for bounded worker transport tests.
    explicit McutMeshBooleanService(QString workerExecutable,int deadlineMilliseconds=30000)
        : m_workerExecutable(std::move(workerExecutable)),m_deadlineMilliseconds(deadlineMilliseconds),m_isolated(true) {}
    MeshBooleanResult unite(const PrintMesh& source,const PrintMesh& additive) override;
    MeshBooleanResult subtract(const PrintMesh& source,const PrintMesh& passage) override;
    QString versionIdentity() const override;
    static bool available();
    static const char* version();
private:
    MeshBooleanResult uniteInWorker(const PrintMesh&,const PrintMesh&);
    MeshBooleanResult subtractInWorker(const PrintMesh&,const PrintMesh&);
    MeshBooleanResult runWorker(const PrintMesh&,const PrintMesh&,bool subtract) const;
    QString m_workerExecutable;
    int m_deadlineMilliseconds=30000;
    bool m_isolated=false;
};
}

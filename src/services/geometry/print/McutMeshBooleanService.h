/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
    explicit McutMeshBooleanService(QString workerExecutable,int deadlineMilliseconds=30000,quint64 memoryBudgetBytes=512ULL*1024*1024)
        : m_workerExecutable(std::move(workerExecutable)),m_deadlineMilliseconds(deadlineMilliseconds),m_memoryBudgetBytes(memoryBudgetBytes),m_isolated(true) {}
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
    quint64 m_memoryBudgetBytes=512ULL*1024*1024;
    bool m_isolated=false;
};
}

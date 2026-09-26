#pragma once

#include "LDrawSourceModel.h"
#include "PartMesh.h"

#include <QDateTime>
#include <QVector>

#include <memory>

namespace LDrawGeometry {

struct LDrawDependencyRecord {
    QString relativePath;
    qint64 size = 0;
    QDateTime modifiedUtc;
};

struct LDrawDependencyFingerprint {
    QVector<LDrawDependencyRecord> dependencies;
};

struct LDrawLoadResult {
    PartMesh mesh;
    std::shared_ptr<LDrawSourceModel> sourceModel;
    LDrawDependencyFingerprint dependencyFingerprint;
    // External roots have no catalog Part identity; dependencies retain library provenance.
    QString externalFilePath;
    QByteArray externalContentHash;
    Error error;
    bool ok() const { return error.code == ErrorCode::None; }
};

} // namespace LDrawGeometry

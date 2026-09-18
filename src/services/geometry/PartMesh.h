#pragma once

#include <QVector>
#include <QVector3D>
#include <QString>
#include <QStringList>

namespace LDrawGeometry {

enum class ErrorCode {
    None,
    LibraryNotConfigured,
    InvalidLibrary,
    NoIdentity,
    AmbiguousIdentity,
    ModelNotFound,
    DependencyMissing,
    TraversalRejected,
    CycleDetected,
    ResourceLimitExceeded,
    MalformedSource,
    Cancelled,
    ExportFailed
};

struct Error {
    ErrorCode code = ErrorCode::None;
    QString message;
    QString reference;
    int line = 0;
};

struct Triangle {
    QVector3D a;
    QVector3D b;
    QVector3D c;
    QVector3D normal;
    QString color;
};

struct Edge {
    QVector3D a;
    QVector3D b;
    QString color;
};

struct ConditionalEdge {
    QVector3D a;
    QVector3D b;
    QVector3D control1;
    QVector3D control2;
    QString color;
};

struct PartMesh {
    QString ldrawId;
    QString sourceRelativePath;
    QString sourceProvenance;
    QVector<Triangle> triangles;
    QVector<Edge> hardEdges;
    QVector<ConditionalEdge> conditionalEdges;
    QVector3D minimumBounds;
    QVector3D maximumBounds;
    bool hasBounds = false;
    bool bfcCertified = false;
    int degenerateFaces = 0;
    int sourceFiles = 0;
    QStringList diagnostics;

    QVector3D dimensionsLdu() const { return maximumBounds - minimumBounds; }
    QVector3D dimensionsMm() const { return dimensionsLdu() * 0.4f; }
};

struct Result {
    PartMesh mesh;
    Error error;
    bool ok() const { return error.code == ErrorCode::None; }
};

struct LibraryValidation {
    bool valid = false;
    QString normalizedRoot;
    QString status;
};

} // namespace LDrawGeometry

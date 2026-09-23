#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include <array>

namespace LDrawGeometry {

enum class SourceClassification { Unknown, Part, Subpart, Primitive };

struct SourceFileRecord {
    int id = -1;
    QString relativePath;
    SourceClassification classification = SourceClassification::Unknown;
    QString description;
};

struct ReferenceRecord {
    int id = -1;
    int parentId = -1;
    int fileId = -1;
    int sourceLine = 0;
    std::array<double, 12> accumulatedTransform{};
    bool mirrored = false;
    bool inverted = false;
};

struct SurfaceRecord {
    int triangleIndex = -1;
    int referenceId = -1;
    int fileId = -1;
    int sourceLine = 0;
    int sourceType = 0;
    bool certified = false;
    bool clipping = true;
    bool inverted = false;
};

struct LDrawSourceModel {
    QVector<SourceFileRecord> files;
    QVector<ReferenceRecord> references;
    QVector<SurfaceRecord> surfaces;
    QHash<QString, int> fileIds;
};

} // namespace LDrawGeometry

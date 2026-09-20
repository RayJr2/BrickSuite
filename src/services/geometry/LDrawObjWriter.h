#pragma once
#include "PartMesh.h"
#include "print/PrintMesh.h"
#include <QString>
class LDrawObjWriter
{
public:
    struct Options {
        double uniformScale = 1.0;
        QString partNumber;
        QString ldrawId;
        QString geometryLabel = QStringLiteral("Source");
    };
    static bool write(const LDrawGeometry::PartMesh& mesh, const QString& path,
                      LDrawGeometry::Error* error = nullptr);
    static bool write(const LDrawGeometry::PartMesh& mesh, const QString& path,
                      const Options& options, LDrawGeometry::Error* error = nullptr);
    static bool write(const PrintGeometry::PrintMesh& mesh, const QString& path,
                      const Options& options, LDrawGeometry::Error* error = nullptr);
};

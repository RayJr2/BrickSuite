#pragma once
#include "PartMesh.h"
#include <QString>
class LDrawObjWriter
{
public:
    struct Options {
        double uniformScale = 1.0;
        QString partNumber;
    };
    static bool write(const LDrawGeometry::PartMesh& mesh, const QString& path,
                      LDrawGeometry::Error* error = nullptr);
    static bool write(const LDrawGeometry::PartMesh& mesh, const QString& path,
                      const Options& options, LDrawGeometry::Error* error = nullptr);
};

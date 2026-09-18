#pragma once
#include "PartMesh.h"
class LDrawObjWriter
{
public:
    static bool write(const LDrawGeometry::PartMesh& mesh, const QString& path,
                      LDrawGeometry::Error* error = nullptr);
};

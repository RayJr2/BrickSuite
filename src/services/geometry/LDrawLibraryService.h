#pragma once

#include "PartMesh.h"

#include <QHash>
#include <QString>

class LDrawLibraryService
{
public:
    static LDrawGeometry::LibraryValidation validateLibrary(const QString& root);
    static LDrawGeometry::Result loadPart(const QString& root, const QString& ldrawId);
};

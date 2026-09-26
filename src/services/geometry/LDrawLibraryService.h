#pragma once

#include "LDrawLoadResult.h"

#include <QHash>
#include <QString>

class LDrawLibraryService
{
public:
    static LDrawGeometry::LibraryValidation validateLibrary(const QString& root);
    static LDrawGeometry::LDrawLoadResult loadExternalFile(const QString& root, const QString& path);
    static LDrawGeometry::LDrawLoadResult loadPart(const QString& root, const QString& ldrawId);
};

#pragma once
#include "PrintMesh.h"
#include <QString>
namespace PrintGeometry { class PreparedObjProofWriter { public: static bool write(const PrintMesh&,const QString&,QString*error=nullptr); }; }

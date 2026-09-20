#pragma once
#include "print/PrintMesh.h"
#include <QString>
class BinaryStlWriter { public: static bool write(const PrintGeometry::PrintMesh&,const QString&,double scale,QString* error=nullptr); };

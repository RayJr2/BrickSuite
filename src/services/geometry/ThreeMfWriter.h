#pragma once
#include "print/PrintMesh.h"
#include <QColor>
#include <QString>
class ThreeMfWriter { public: struct Options { double uniformScale=1.0; QString objectName; QString partIdentity; QColor modelColor; }; static bool write(const PrintGeometry::PrintMesh&,const QString&,const Options&,QString* error=nullptr); };

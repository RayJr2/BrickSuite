#pragma once
#include "print/PrintMesh.h"
#include <QColor>
#include <QString>
#include <QVector>
#include <functional>
class ThreeMfWriter { public: struct Options { double uniformScale=1.0; QString objectName; QString partIdentity; QColor modelColor; std::function<void(const QString&)> phase; }; struct NamedMesh { QString name; PrintGeometry::PrintMesh mesh; PrintGeometry::Point translation; }; static bool write(const PrintGeometry::PrintMesh&,const QString&,const Options&,QString* error=nullptr); static bool writeCollection(const QVector<NamedMesh>&,const QString&,const Options&,QString* error=nullptr); };

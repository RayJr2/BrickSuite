#pragma once

#include "PartMesh.h"
#include <QMatrix4x4>

class ConditionalEdgeVisibility
{
public:
    static bool isVisible(const LDrawGeometry::ConditionalEdge& edge,
                          const QMatrix4x4& modelViewProjection,
                          float epsilon = 1.0e-6f);
};

#include "ConditionalEdgeVisibility.h"

#include <QVector2D>
#include <QVector4D>
#include <cmath>

namespace {
bool project(const QMatrix4x4& matrix, const QVector3D& point, QVector2D* result)
{
    const QVector4D clip = matrix * QVector4D(point, 1.0f);
    if (!std::isfinite(clip.w()) || qAbs(clip.w()) < 1.0e-7f)
        return false;
    *result = QVector2D(clip.x() / clip.w(), clip.y() / clip.w());
    return std::isfinite(result->x()) && std::isfinite(result->y());
}

float side(const QVector2D& a, const QVector2D& b, const QVector2D& point)
{ return (b.x()-a.x())*(point.y()-a.y())-(b.y()-a.y())*(point.x()-a.x()); }
}

bool ConditionalEdgeVisibility::isVisible(const LDrawGeometry::ConditionalEdge& edge,
                                           const QMatrix4x4& matrix, float epsilon)
{
    QVector2D a,b,c1,c2;
    if (!project(matrix,edge.a,&a) || !project(matrix,edge.b,&b)
        || !project(matrix,edge.control1,&c1) || !project(matrix,edge.control2,&c2))
        return false;
    if ((b-a).lengthSquared() < epsilon*epsilon)
        return false;
    const float first=side(a,b,c1), second=side(a,b,c2);
    if (qAbs(first)<=epsilon || qAbs(second)<=epsilon)
        return true;
    return (first>0.0f)==(second>0.0f);
}

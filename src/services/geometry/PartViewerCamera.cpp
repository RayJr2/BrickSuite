#include "PartViewerCamera.h"

#include <QtMath>
#include <algorithm>

void PartViewerCamera::setBounds(const QVector3D& minimum,const QVector3D& maximum,bool resetTarget)
{ m_minimum=minimum; m_maximum=maximum; if(resetTarget)m_target=(minimum+maximum)*0.5f; }

void PartViewerCamera::setViewport(float width,float height)
{ m_width=qMax(1.0f,width); m_height=qMax(1.0f,height); }

void PartViewerCamera::setScale(float scale)
{ m_scale=qBound(0.01f,scale,10.0f); }

void PartViewerCamera::setProjection(Projection projection)
{
    if(m_projection==projection) return;
    const float radians=qDegreesToRadians(m_fieldOfViewDegrees*0.5f);
    if(projection==Projection::Orthographic)
        m_orthographicHeight=qMax(0.001f,2.0f*m_distance*qTan(radians));
    else
        m_distance=qMax(0.001f,m_orthographicHeight/(2.0f*qTan(radians)));
    m_projection=projection;
}

void PartViewerCamera::setView(View view)
{
    switch(view){
    case View::Isometric:setLookDirection(QVector3D(1,-1,1).normalized(),QVector3D(0,-1,0));break;
    case View::Front:setLookDirection({0,0,-1},{0,-1,0});break;
    case View::Back:setLookDirection({0,0,1},{0,-1,0});break;
    case View::Left:setLookDirection({-1,0,0},{0,-1,0});break;
    case View::Right:setLookDirection({1,0,0},{0,-1,0});break;
    case View::Top:setLookDirection({0,-1,0},{0,0,-1});break;
    case View::Bottom:setLookDirection({0,1,0},{0,0,1});break;
    }
}

void PartViewerCamera::fit()
{
    m_target=(m_minimum+m_maximum)*0.5f;
    const QVector3D extent=(m_maximum-m_minimum)*m_scale;
    const float radius=qMax(0.001f,0.5f*extent.length());
    const float aspect=m_width/m_height;
    const float vertical=qDegreesToRadians(m_fieldOfViewDegrees*0.5f);
    const float horizontal=qAtan(qTan(vertical)*aspect);
    const float limiting=qMin(vertical,horizontal);
    m_distance=qMax(0.01f,radius/qSin(qMax(0.01f,limiting))*1.12f);
    m_orthographicHeight=qMax(0.01f,2.0f*radius*qMax(1.0f,1.0f/aspect)*1.12f);
}

void PartViewerCamera::orbit(const QPointF& previousPosition,const QPointF& currentPosition)
{
    const QVector3D from=trackballPoint(previousPosition),to=trackballPoint(currentPosition);
    const QVector3D axis=QVector3D::crossProduct(from,to);
    if(axis.lengthSquared()<1.0e-10f)return;
    const float dot=qBound(-1.0f,QVector3D::dotProduct(from,to),1.0f);
    const QQuaternion drag=QQuaternion::fromAxisAndAngle(axis.normalized(),qRadiansToDegrees(qAcos(dot)));
    m_orientation=(m_orientation*drag.conjugated()).normalized();
}

void PartViewerCamera::pan(float dx,float dy)
{
    const float worldPerPixel=(m_projection==Projection::Orthographic
        ? m_orthographicHeight : 2.0f*m_distance*qTan(qDegreesToRadians(m_fieldOfViewDegrees*0.5f))) / m_height;
    m_target += rightDirection()*(-dx*worldPerPixel)+upDirection()*(dy*worldPerPixel);
}

void PartViewerCamera::zoom(float steps)
{
    const float factor=qPow(0.85f,steps);
    m_distance=qBound(0.001f,m_distance*factor,1.0e8f);
    m_orthographicHeight=qBound(0.001f,m_orthographicHeight*factor,1.0e8f);
}

QVector3D PartViewerCamera::eyeDirection() const
{ return m_orientation.rotatedVector({0,0,1}).normalized(); }

QVector3D PartViewerCamera::rightDirection() const
{ return m_orientation.rotatedVector({1,0,0}).normalized(); }

QVector3D PartViewerCamera::upDirection() const
{ return m_orientation.rotatedVector({0,1,0}).normalized(); }

QVector3D PartViewerCamera::trackballPoint(const QPointF& position) const
{
    const float diameter=qMax(1.0f,qMin(m_width,m_height));
    float x=float((2.0*position.x()-m_width)/diameter);
    float y=float((m_height-2.0*position.y())/diameter);
    const float lengthSquared=x*x+y*y;
    if(lengthSquared>1.0f){const float inverse=1.0f/qSqrt(lengthSquared);x*=inverse;y*=inverse;return {x,y,0};}
    return {x,y,qSqrt(1.0f-lengthSquared)};
}

void PartViewerCamera::setLookDirection(const QVector3D& eye,const QVector3D& requestedUp)
{
    const QVector3D forward=eye.normalized();
    const QVector3D right=QVector3D::crossProduct(requestedUp,forward).normalized();
    const QVector3D up=QVector3D::crossProduct(forward,right).normalized();
    QMatrix3x3 matrix;
    matrix(0,0)=right.x();matrix(1,0)=right.y();matrix(2,0)=right.z();
    matrix(0,1)=up.x();matrix(1,1)=up.y();matrix(2,1)=up.z();
    matrix(0,2)=forward.x();matrix(1,2)=forward.y();matrix(2,2)=forward.z();
    m_orientation=QQuaternion::fromRotationMatrix(matrix).normalized();
}

QMatrix4x4 PartViewerCamera::viewMatrix() const
{
    QMatrix4x4 value; const QVector3D eye=m_target+eyeDirection()*m_distance;
    value.lookAt(eye,m_target,upDirection()); return value;
}

QMatrix4x4 PartViewerCamera::projectionMatrix() const
{
    QMatrix4x4 value; const float aspect=m_width/m_height;
    if(m_projection==Projection::Perspective)
        value.perspective(m_fieldOfViewDegrees,aspect,qMax(0.001f,m_distance/1000.0f),m_distance*1000.0f);
    else { const float half=m_orthographicHeight*0.5f; value.ortho(-half*aspect,half*aspect,-half,half,-1.0e7f,1.0e7f); }
    return value;
}

QMatrix4x4 PartViewerCamera::modelMatrix() const
{ QMatrix4x4 value; value.scale(m_scale); return value; }
QMatrix4x4 PartViewerCamera::modelViewMatrix() const
{ return viewMatrix()*modelMatrix(); }
QMatrix4x4 PartViewerCamera::modelViewProjection() const
{ return projectionMatrix()*viewMatrix()*modelMatrix(); }
QMatrix4x4 PartViewerCamera::rotationViewMatrix() const
{
    QMatrix4x4 value;
    value.lookAt(eyeDirection(),QVector3D(),upDirection());
    return value;
}

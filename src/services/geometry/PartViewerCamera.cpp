#include "PartViewerCamera.h"

#include <QtMath>
#include <algorithm>

void PartViewerCamera::setBounds(const QVector3D& minimum,const QVector3D& maximum)
{ m_minimum=minimum; m_maximum=maximum; m_target=(minimum+maximum)*0.5f; }

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
    case View::Isometric:m_yawDegrees=45;m_pitchDegrees=30;break;
    case View::Front:m_yawDegrees=0;m_pitchDegrees=0;break;
    case View::Back:m_yawDegrees=180;m_pitchDegrees=0;break;
    case View::Left:m_yawDegrees=-90;m_pitchDegrees=0;break;
    case View::Right:m_yawDegrees=90;m_pitchDegrees=0;break;
    case View::Top:m_yawDegrees=0;m_pitchDegrees=89.9f;break;
    case View::Bottom:m_yawDegrees=0;m_pitchDegrees=-89.9f;break;
    }
}

void PartViewerCamera::fit()
{
    const QVector3D extent=(m_maximum-m_minimum)*m_scale;
    const float radius=qMax(0.001f,0.5f*extent.length());
    const float aspect=m_width/m_height;
    const float vertical=qDegreesToRadians(m_fieldOfViewDegrees*0.5f);
    const float horizontal=qAtan(qTan(vertical)*aspect);
    const float limiting=qMin(vertical,horizontal);
    m_distance=qMax(0.01f,radius/qSin(qMax(0.01f,limiting))*1.12f);
    m_orthographicHeight=qMax(0.01f,2.0f*radius*qMax(1.0f,1.0f/aspect)*1.12f);
}

void PartViewerCamera::orbit(float dx,float dy)
{ m_yawDegrees+=dx; m_pitchDegrees=qBound(-89.9f,m_pitchDegrees+dy,89.9f); }

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
{
    const float yaw=qDegreesToRadians(m_yawDegrees), pitch=qDegreesToRadians(m_pitchDegrees);
    return QVector3D(qCos(pitch)*qSin(yaw),qSin(pitch),qCos(pitch)*qCos(yaw)).normalized();
}

QVector3D PartViewerCamera::rightDirection() const
{ return QVector3D::crossProduct(QVector3D(0,1,0),eyeDirection()).normalized(); }

QVector3D PartViewerCamera::upDirection() const
{ return QVector3D::crossProduct(eyeDirection(),rightDirection()).normalized(); }

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

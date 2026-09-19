#pragma once

#include <QMatrix4x4>
#include <QPointF>
#include <QQuaternion>
#include <QVector3D>

class PartViewerCamera
{
public:
    enum class Projection { Perspective, Orthographic };
    enum class View { Isometric, Front, Back, Left, Right, Top, Bottom };

    void setBounds(const QVector3D& minimum, const QVector3D& maximum, bool resetTarget = true);
    void setViewport(float width, float height);
    void setScale(float scale);
    void setProjection(Projection projection);
    Projection projection() const { return m_projection; }
    void setView(View view);
    void fit();
    void orbit(const QPointF& previousPosition, const QPointF& currentPosition);
    void pan(float deltaX, float deltaY);
    void zoom(float wheelSteps);

    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 modelViewMatrix() const;
    QMatrix4x4 projectionMatrix() const;
    QMatrix4x4 modelMatrix() const;
    QMatrix4x4 modelViewProjection() const;
    QMatrix4x4 rotationViewMatrix() const;
    QVector3D target() const { return m_target; }
    float distance() const { return m_distance; }
    float orthographicHeight() const { return m_orthographicHeight; }
    float scale() const { return m_scale; }
    QQuaternion orientation() const { return m_orientation; }
    QMatrix3x3 orientationMatrix() const { return m_orientation.toRotationMatrix(); }
    QVector3D viewDirection() const { return eyeDirection(); }
    QVector3D viewUpDirection() const { return upDirection(); }

private:
    QVector3D eyeDirection() const;
    QVector3D rightDirection() const;
    QVector3D upDirection() const;
    QVector3D trackballPoint(const QPointF& position) const;
    void setLookDirection(const QVector3D& eyeDirection, const QVector3D& upDirection);

    QVector3D m_minimum{-1,-1,-1};
    QVector3D m_maximum{1,1,1};
    QVector3D m_target;
    float m_width=1.0f;
    float m_height=1.0f;
    float m_scale=1.0f;
    QQuaternion m_orientation;
    float m_distance=10.0f;
    float m_orthographicHeight=10.0f;
    float m_fieldOfViewDegrees=35.0f;
    Projection m_projection=Projection::Perspective;
};

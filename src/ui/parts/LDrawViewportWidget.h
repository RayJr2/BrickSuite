#pragma once

#include "../../services/geometry/PartMesh.h"
#include "../../services/geometry/PartViewerCamera.h"
#include "../../services/geometry/PartViewerState.h"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>

class QOpenGLShaderProgram;

class LDrawViewportWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    explicit LDrawViewportWidget(QWidget* parent=nullptr);
    ~LDrawViewportWidget() override;
    void setMesh(const LDrawGeometry::PartMesh& mesh, bool resetCamera);
    void clearMesh();
    void setUniformScale(float scale);
    void setRenderMode(PartViewerRenderMode mode);
    void setProjection(PartViewerCamera::Projection projection);
    void setStandardView(PartViewerCamera::View view);
    void fitModel();
    bool renderingAvailable()const{return m_ready;}
signals:
    void renderingError(const QString& message);
protected:
    void initializeGL()override;
    void resizeGL(int width,int height)override;
    void paintGL()override;
    void mousePressEvent(QMouseEvent* event)override;
    void mouseMoveEvent(QMouseEvent* event)override;
    void wheelEvent(QWheelEvent* event)override;
private:
    struct Vertex{float px,py,pz,nx,ny,nz,r,g,b,a;};
    void destroyResources();
    void uploadMesh();
    void uploadBuffer(QOpenGLBuffer& buffer,const QVector<Vertex>& vertices);
    void bindAttributes(QOpenGLBuffer& buffer);
    void drawFaces();
    void drawLines();
    QVector<Vertex> conditionalLineVertices()const;
    static Vertex vertex(const QVector3D& p,const QVector3D& n,const QVector4D& c);

    LDrawGeometry::PartMesh m_mesh;
    PartViewerCamera m_camera;
    PartViewerRenderMode m_mode=PartViewerRenderMode::SolidEdges;
    QOpenGLShaderProgram* m_program=nullptr;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_faces{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_wire{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_hardEdges{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_conditionalEdges{QOpenGLBuffer::VertexBuffer};
    int m_culledFaceVertices=0;
    int m_twoSidedFaceVertices=0;
    int m_wireVertices=0;
    int m_hardEdgeVertices=0;
    bool m_ready=false;
    bool m_meshDirty=false;
    QPoint m_lastMouse;
};

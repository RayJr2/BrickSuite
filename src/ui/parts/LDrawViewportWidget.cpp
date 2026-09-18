#include "LDrawViewportWidget.h"

#include "../../services/geometry/ConditionalEdgeVisibility.h"
#include "../../services/geometry/LDrawColorResolver.h"

#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QSurfaceFormat>
#include <QWheelEvent>
#include <cstddef>

namespace {
constexpr auto VertexShader=R"(
#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 color;
uniform mat4 mvp;
uniform mat3 normalMatrix;
out vec3 viewNormal;
out vec4 vertexColor;
void main(){gl_Position=mvp*vec4(position,1.0);viewNormal=normalMatrix*normal;vertexColor=color;}
)";
constexpr auto FragmentShader=R"(
#version 330 core
in vec3 viewNormal;
in vec4 vertexColor;
uniform bool lighting;
uniform bool overrideColor;
uniform vec4 inspectionColor;
out vec4 fragmentColor;
void main(){vec4 baseColor=overrideColor?inspectionColor:vertexColor;float value=1.0;if(lighting){vec3 n=normalize(viewNormal);if(!gl_FrontFacing)n=-n;float key=max(dot(n,normalize(vec3(0.45,0.55,0.75))),0.0);float fill=max(dot(n,normalize(vec3(-0.65,0.30,0.45))),0.0);value=clamp(0.38+0.48*key+0.18*fill,0.0,1.04);}fragmentColor=vec4(baseColor.rgb*value,baseColor.a);}
)";
}

LDrawViewportWidget::LDrawViewportWidget(QWidget* parent):QOpenGLWidget(parent)
{
    QSurfaceFormat format;format.setVersion(3,3);format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);format.setSamples(4);setFormat(format);setMinimumSize(500,350);
    setFocusPolicy(Qt::StrongFocus);
}

LDrawViewportWidget::~LDrawViewportWidget()
{ if(context()){makeCurrent();destroyResources();doneCurrent();} }

void LDrawViewportWidget::setMesh(const LDrawGeometry::PartMesh& mesh,bool resetCamera)
{
    m_mesh=mesh;m_meshDirty=true;
    if(mesh.hasBounds){m_camera.setBounds(mesh.minimumBounds,mesh.maximumBounds);if(resetCamera){m_camera.setProjection(PartViewerCamera::Projection::Perspective);m_camera.setView(PartViewerCamera::View::Isometric);m_camera.fit();}}
    update();
}
void LDrawViewportWidget::clearMesh(){m_mesh={};m_meshDirty=true;update();}
void LDrawViewportWidget::setUniformScale(float scale){m_camera.setScale(scale);update();}
void LDrawViewportWidget::setRenderMode(PartViewerRenderMode mode){m_mode=mode;update();}
void LDrawViewportWidget::setProjection(PartViewerCamera::Projection value){m_camera.setProjection(value);update();}
void LDrawViewportWidget::setStandardView(PartViewerCamera::View view){m_camera.setView(view);update();}
void LDrawViewportWidget::fitModel(){m_camera.fit();update();}

void LDrawViewportWidget::initializeGL()
{
    if(!initializeOpenGLFunctions()||!context()
       ||context()->format().majorVersion()<3
       ||(context()->format().majorVersion()==3&&context()->format().minorVersion()<3)
       ||context()->format().profile()!=QSurfaceFormat::CoreProfile){
        emit renderingError(tr("OpenGL 3.3 Core is unavailable."));return;
    }
    m_program=new QOpenGLShaderProgram;
    if(!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,VertexShader)
       ||!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,FragmentShader)
       ||!m_program->link()){
        const QString error=m_program->log();delete m_program;m_program=nullptr;
        emit renderingError(tr("The 3D shaders could not be initialized: %1").arg(error));return;
    }
    m_vao.create();m_faces.create();m_wire.create();m_hardEdges.create();m_conditionalEdges.create();
    glEnable(GL_DEPTH_TEST);glEnable(GL_MULTISAMPLE);glClearColor(0.075f,0.085f,0.10f,1.0f);
    m_ready=true;m_meshDirty=true;
}

void LDrawViewportWidget::resizeGL(int width,int height)
{ glViewport(0,0,width,height);m_camera.setViewport(float(width),float(height)); }

void LDrawViewportWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    if(!m_ready||!m_program)return;
    if(m_meshDirty)uploadMesh();
    m_program->bind();m_program->setUniformValue("mvp",m_camera.modelViewProjection());
    m_program->setUniformValue("normalMatrix",m_camera.modelViewMatrix().normalMatrix());
    const PartViewerRenderPasses passes=partViewerRenderPasses(m_mode);
    if(passes.depthPrepass){
        glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
        drawFaces();
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    }else if(passes.visibleFaces)drawFaces();
    if(passes.triangleEdges||passes.ldrawEdges)drawLines();
    m_program->release();
}

void LDrawViewportWidget::destroyResources()
{
    m_faces.destroy();m_wire.destroy();m_hardEdges.destroy();m_conditionalEdges.destroy();m_vao.destroy();
    delete m_program;m_program=nullptr;m_ready=false;
}

LDrawViewportWidget::Vertex LDrawViewportWidget::vertex(const QVector3D&p,const QVector3D&n,const QVector4D&c)
{return{p.x(),p.y(),p.z(),n.x(),n.y(),n.z(),c.x(),c.y(),c.z(),c.w()};}

void LDrawViewportWidget::uploadBuffer(QOpenGLBuffer& buffer,const QVector<Vertex>& vertices)
{buffer.bind();buffer.setUsagePattern(&buffer==&m_conditionalEdges?QOpenGLBuffer::DynamicDraw:QOpenGLBuffer::StaticDraw);buffer.allocate(vertices.constData(),vertices.size()*int(sizeof(Vertex)));buffer.release();}

void LDrawViewportWidget::uploadMesh()
{
    QVector<Vertex> culled,twoSided,wire,hard;
    for(const auto&t:m_mesh.triangles){auto&target=t.backFaceCull?culled:twoSided;const auto color=LDrawColorResolver::faceColor(t.color);target<<vertex(t.a,t.normal,color)<<vertex(t.b,t.normal,color)<<vertex(t.c,t.normal,color);const auto edge=LDrawColorResolver::edgeColor(t.color);const QVector3D n;wire<<vertex(t.a,n,edge)<<vertex(t.b,n,edge)<<vertex(t.b,n,edge)<<vertex(t.c,n,edge)<<vertex(t.c,n,edge)<<vertex(t.a,n,edge);}
    culled+=twoSided;m_culledFaceVertices=culled.size()-twoSided.size();m_twoSidedFaceVertices=twoSided.size();
    for(const auto&e:m_mesh.hardEdges){const auto color=LDrawColorResolver::edgeColor(e.color);hard<<vertex(e.a,{},color)<<vertex(e.b,{},color);}
    uploadBuffer(m_faces,culled);uploadBuffer(m_wire,wire);uploadBuffer(m_hardEdges,hard);
    m_wireVertices=wire.size();m_hardEdgeVertices=hard.size();m_meshDirty=false;
}

void LDrawViewportWidget::bindAttributes(QOpenGLBuffer& buffer)
{
    m_vao.bind();buffer.bind();
    m_program->enableAttributeArray(0);m_program->setAttributeBuffer(0,GL_FLOAT,offsetof(Vertex,px),3,sizeof(Vertex));
    m_program->enableAttributeArray(1);m_program->setAttributeBuffer(1,GL_FLOAT,offsetof(Vertex,nx),3,sizeof(Vertex));
    m_program->enableAttributeArray(2);m_program->setAttributeBuffer(2,GL_FLOAT,offsetof(Vertex,r),4,sizeof(Vertex));
}

void LDrawViewportWidget::drawFaces()
{
    bindAttributes(m_faces);m_program->setUniformValue("lighting",true);m_program->setUniformValue("overrideColor",false);
    glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(1.0f,1.0f);
    glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glDrawArrays(GL_TRIANGLES,0,m_culledFaceVertices);
    glDisable(GL_CULL_FACE);glDrawArrays(GL_TRIANGLES,m_culledFaceVertices,m_twoSidedFaceVertices);
    glDisable(GL_POLYGON_OFFSET_FILL);m_faces.release();m_vao.release();
}

QVector<LDrawViewportWidget::Vertex> LDrawViewportWidget::conditionalLineVertices()const
{
    QVector<Vertex> result;const QMatrix4x4 matrix=m_camera.modelViewProjection();
    for(const auto&e:m_mesh.conditionalEdges)if(ConditionalEdgeVisibility::isVisible(e,matrix)){const auto color=LDrawColorResolver::edgeColor(e.color);result<<vertex(e.a,{},color)<<vertex(e.b,{},color);}
    return result;
}

void LDrawViewportWidget::drawLines()
{
    m_program->setUniformValue("lighting",false);
    const PartViewerRenderPasses passes=partViewerRenderPasses(m_mode);
    const bool wireframe=m_mode==PartViewerRenderMode::Wireframe;
    m_program->setUniformValue("overrideColor",wireframe);
    if(wireframe)m_program->setUniformValue("inspectionColor",LDrawColorResolver::wireframeColor());
    if(passes.triangleEdges){bindAttributes(m_wire);glDrawArrays(GL_LINES,0,m_wireVertices);m_wire.release();m_vao.release();}
    if(passes.ldrawEdges){
        bindAttributes(m_hardEdges);glDrawArrays(GL_LINES,0,m_hardEdgeVertices);m_hardEdges.release();m_vao.release();
        const auto conditional=conditionalLineVertices();uploadBuffer(m_conditionalEdges,conditional);bindAttributes(m_conditionalEdges);glDrawArrays(GL_LINES,0,conditional.size());m_conditionalEdges.release();m_vao.release();
    }
}

void LDrawViewportWidget::mousePressEvent(QMouseEvent*event){m_lastMouse=event->position().toPoint();event->accept();}
void LDrawViewportWidget::mouseMoveEvent(QMouseEvent*event)
{
    const QPoint current=event->position().toPoint(),delta=current-m_lastMouse;m_lastMouse=current;
    if((event->buttons()&Qt::MiddleButton)||((event->buttons()&Qt::LeftButton)&&(event->modifiers()&Qt::ShiftModifier)))m_camera.pan(delta.x(),delta.y());
    else if(event->buttons()&Qt::LeftButton)m_camera.orbit(delta.x()*0.45f,delta.y()*0.45f);
    update();event->accept();
}
void LDrawViewportWidget::wheelEvent(QWheelEvent*event){m_camera.zoom(event->angleDelta().y()/120.0f);update();event->accept();}

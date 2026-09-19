#include "LDrawViewportWidget.h"

#include "../../services/geometry/ConditionalEdgeVisibility.h"
#include "../../services/geometry/LDrawColorResolver.h"

#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QPainter>
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
    if(mesh.hasBounds){m_camera.setBounds(mesh.minimumBounds,mesh.maximumBounds,resetCamera);if(resetCamera){m_camera.setProjection(PartViewerCamera::Projection::Perspective);m_camera.setView(PartViewerCamera::View::Isometric);m_camera.fit();}}
    update();
}
void LDrawViewportWidget::clearMesh(){m_mesh={};m_meshDirty=true;update();}
void LDrawViewportWidget::setUniformScale(float scale){m_camera.setScale(scale);update();}
void LDrawViewportWidget::setRenderMode(PartViewerRenderMode mode){m_mode=mode;update();}
void LDrawViewportWidget::setProjection(PartViewerCamera::Projection value){m_camera.setProjection(value);update();}
void LDrawViewportWidget::setStandardView(PartViewerCamera::View view){m_camera.setView(view);update();}
void LDrawViewportWidget::fitModel(){m_camera.fit();update();}
void LDrawViewportWidget::resetView(){m_camera.setView(PartViewerCamera::View::Isometric);m_camera.fit();update();}
void LDrawViewportWidget::setShowAxes(bool show){m_showAxes=show;update();}
void LDrawViewportWidget::setModelColor(const QColor& color){if(!color.isValid()||color==m_modelColor)return;m_modelColor=color;m_meshDirty=true;update();}

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
    m_vao.create();m_faces.create();m_wire.create();m_hardEdges.create();m_conditionalEdges.create();m_axes.create();
    const QVector3D n;const QVector4D red(0.95f,0.20f,0.20f,1),green(0.25f,0.90f,0.30f,1),blue(0.25f,0.50f,1,1);
    QVector<Vertex> axes;
    const auto addAxis=[&axes,&n](const QVector3D&end,const QVector3D&a,const QVector3D&b,const QVector4D&color){axes<<vertex({},n,color)<<vertex(end,n,color)<<vertex(end,n,color)<<vertex(a,n,color)<<vertex(end,n,color)<<vertex(b,n,color);};
    addAxis(PartViewerAxisConvention::x(),{0.78f,0.10f,0},{0.78f,-0.10f,0},red);
    addAxis(PartViewerAxisConvention::y(),{0.10f,0,0.78f},{-0.10f,0,0.78f},green);
    addAxis(PartViewerAxisConvention::z(),{0.10f,-0.78f,0},{-0.10f,-0.78f,0},blue);
    uploadBuffer(m_axes,axes);
    glEnable(GL_DEPTH_TEST);glEnable(GL_MULTISAMPLE);glClearColor(0.075f,0.085f,0.10f,1.0f);
    m_ready=true;m_meshDirty=true;
}

void LDrawViewportWidget::resizeGL(int width,int height)
{ glViewport(0,0,width,height);m_camera.setViewport(float(width),float(height)); }

void LDrawViewportWidget::paintGL()
{
    establishMainRenderState();
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
    if(m_showAxes){drawAxes();drawAxisLabels();}
}

void LDrawViewportWidget::establishMainRenderState()
{
    const qreal ratio=devicePixelRatioF();
    glViewport(0,0,qRound(width()*ratio),qRound(height()*ratio));
    glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glFrontFace(GL_CCW);glCullFace(GL_BACK);glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
    glLineWidth(1.0f);
}

void LDrawViewportWidget::destroyResources()
{
    m_faces.destroy();m_wire.destroy();m_hardEdges.destroy();m_conditionalEdges.destroy();m_axes.destroy();m_vao.destroy();
    delete m_program;m_program=nullptr;m_ready=false;
}

LDrawViewportWidget::Vertex LDrawViewportWidget::vertex(const QVector3D&p,const QVector3D&n,const QVector4D&c)
{return{p.x(),p.y(),p.z(),n.x(),n.y(),n.z(),c.x(),c.y(),c.z(),c.w()};}

void LDrawViewportWidget::uploadBuffer(QOpenGLBuffer& buffer,const QVector<Vertex>& vertices)
{buffer.bind();buffer.setUsagePattern(&buffer==&m_conditionalEdges?QOpenGLBuffer::DynamicDraw:QOpenGLBuffer::StaticDraw);buffer.allocate(vertices.constData(),vertices.size()*int(sizeof(Vertex)));buffer.release();}

void LDrawViewportWidget::uploadMesh()
{
    QVector<Vertex> culled,twoSided,wire,hard;
    for(const auto&t:m_mesh.triangles){auto&target=t.backFaceCull?culled:twoSided;const auto color=LDrawColorResolver::faceColor(t.color,m_modelColor);target<<vertex(t.a,t.normal,color)<<vertex(t.b,t.normal,color)<<vertex(t.c,t.normal,color);const auto edge=LDrawColorResolver::edgeColor(t.color,m_modelColor);const QVector3D n;wire<<vertex(t.a,n,edge)<<vertex(t.b,n,edge)<<vertex(t.b,n,edge)<<vertex(t.c,n,edge)<<vertex(t.c,n,edge)<<vertex(t.a,n,edge);}
    culled+=twoSided;m_culledFaceVertices=culled.size()-twoSided.size();m_twoSidedFaceVertices=twoSided.size();
    for(const auto&e:m_mesh.hardEdges){const auto color=LDrawColorResolver::edgeColor(e.color,m_modelColor);hard<<vertex(e.a,{},color)<<vertex(e.b,{},color);}
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
    for(const auto&e:m_mesh.conditionalEdges)if(ConditionalEdgeVisibility::isVisible(e,matrix)){const auto color=LDrawColorResolver::edgeColor(e.color,m_modelColor);result<<vertex(e.a,{},color)<<vertex(e.b,{},color);}
    return result;
}

void LDrawViewportWidget::drawAxes()
{
    const qreal ratio=devicePixelRatioF();const int side=qRound(92*ratio),margin=qRound(12*ratio);
    glViewport(margin,margin,side,side);glDisable(GL_DEPTH_TEST);
    QMatrix4x4 projection;projection.ortho(-1.35f,1.35f,-1.35f,1.35f,-10.0f,10.0f);
    m_program->bind();m_program->setUniformValue("mvp",projection*m_camera.rotationViewMatrix());
    m_program->setUniformValue("normalMatrix",QMatrix3x3());m_program->setUniformValue("lighting",false);m_program->setUniformValue("overrideColor",false);
    bindAttributes(m_axes);glDrawArrays(GL_LINES,0,18);m_axes.release();m_vao.release();m_program->release();
    glEnable(GL_DEPTH_TEST);glViewport(0,0,qRound(width()*ratio),qRound(height()*ratio));
}

void LDrawViewportWidget::drawAxisLabels()
{
    constexpr qreal side=92.0,margin=12.0;
    QMatrix4x4 projection;projection.ortho(-1.35f,1.35f,-1.35f,1.35f,-10.0f,10.0f);
    const QMatrix4x4 matrix=projection*m_camera.rotationViewMatrix();
    const auto position=[this,&matrix](const QVector3D&axis){
        const QVector4D clip=matrix*QVector4D(axis*1.16f,1.0f);const qreal x=clip.x()/clip.w(),y=clip.y()/clip.w();
        return QPointF(margin+(x+1.0)*side*0.5,height()-(margin+(y+1.0)*side*0.5));
    };
    QPainter painter(this);painter.setRenderHint(QPainter::TextAntialiasing,true);QFont font=painter.font();font.setBold(true);font.setPixelSize(14);painter.setFont(font);
    const auto label=[&painter,&position](const QString&text,const QVector3D&axis,const QColor&color){const QPointF point=position(axis);painter.setPen(color);painter.drawText(QRectF(point.x()-9,point.y()-9,18,18),Qt::AlignCenter,text);};
    label(QStringLiteral("X"),PartViewerAxisConvention::x(),QColor("#F23B3B"));
    label(QStringLiteral("Y"),PartViewerAxisConvention::y(),QColor("#35D948"));
    label(QStringLiteral("Z"),PartViewerAxisConvention::z(),QColor("#4384FF"));
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

void LDrawViewportWidget::mousePressEvent(QMouseEvent*event){m_lastMouse=event->position();event->accept();}
void LDrawViewportWidget::mouseMoveEvent(QMouseEvent*event)
{
    const QPointF current=event->position(),delta=current-m_lastMouse;
    if((event->buttons()&Qt::MiddleButton)||((event->buttons()&Qt::LeftButton)&&(event->modifiers()&Qt::ShiftModifier)))m_camera.pan(delta.x(),delta.y());
    else if(event->buttons()&Qt::LeftButton)m_camera.orbit(m_lastMouse,current);
    m_lastMouse=current;
    update();event->accept();
}
void LDrawViewportWidget::wheelEvent(QWheelEvent*event){m_camera.zoom(event->angleDelta().y()/120.0f);update();event->accept();}

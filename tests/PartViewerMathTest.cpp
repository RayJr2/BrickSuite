#include "../src/services/geometry/ConditionalEdgeVisibility.h"
#include "../src/services/geometry/LDrawColorResolver.h"
#include "../src/services/geometry/PartViewerCamera.h"
#include "../src/services/geometry/PartViewerState.h"

#include <QCoreApplication>
#include <QDebug>
#include <cmath>

namespace { bool require(bool value,const char*message){if(!value)qCritical("%s",message);return value;} }

int main(int argc,char**argv)
{
    QCoreApplication app(argc,argv);bool ok=true;
    LDrawGeometry::ConditionalEdge edge{{-1,0,0},{1,0,0},{0,1,0},{0,2,0},"24"};
    QMatrix4x4 identity;
    ok&=require(ConditionalEdgeVisibility::isVisible(edge,identity),"same-side conditional edge visible");
    edge.control2={0,-1,0};ok&=require(!ConditionalEdgeVisibility::isVisible(edge,identity),"opposite-side conditional edge hidden");
    edge.control2={0,0,0};ok&=require(ConditionalEdgeVisibility::isVisible(edge,identity),"edge-on transition deterministic and visible");
    std::swap(edge.a,edge.b);ok&=require(ConditionalEdgeVisibility::isVisible(edge,identity),"reversed endpoints retain visibility");
    edge={{-1,0,0},{1,0,0},{0,1,0},{0,2,0},"24"};QMatrix4x4 perspective;perspective.perspective(45,1,0.1f,100);perspective.translate(0,0,-5);
    ok&=require(ConditionalEdgeVisibility::isVisible(edge,perspective),"perspective conditional edge visible");
    QMatrix4x4 mirrored;mirrored.scale(-1,1,1);ok&=require(ConditionalEdgeVisibility::isVisible(edge,mirrored),"mirrored view retains visibility");
    QMatrix4x4 orthographic;orthographic.ortho(-2,2,-2,2,-10,10);edge.control2={0,-1,0};ok&=require(!ConditionalEdgeVisibility::isVisible(edge,orthographic),"orthographic opposite-side edge hidden");

    PartViewerCamera camera;camera.setViewport(800,600);camera.setBounds({-40,-20,-14},{40,20,14});camera.setScale(1);camera.setView(PartViewerCamera::View::Isometric);camera.fit();
    const float fitted=camera.distance();ok&=require(fitted>0&&camera.target()==QVector3D(),"fit establishes positive distance at bounds center");camera.orbit(10,-5);const QVector3D beforePan=camera.target();camera.pan(5,-3);ok&=require(camera.target()!=beforePan,"pan updates target");camera.zoom(1);ok&=require(camera.distance()<fitted,"zoom changes distance");
    const float perspectiveDistance=camera.distance();camera.setProjection(PartViewerCamera::Projection::Orthographic);ok&=require(camera.orthographicHeight()>0,"projection switch preserves framing");camera.setProjection(PartViewerCamera::Projection::Perspective);ok&=require(std::abs(camera.distance()-perspectiveDistance)<0.01f,"projection round trip preserves framing");camera.setProjection(PartViewerCamera::Projection::Orthographic);camera.setScale(2);camera.fit();ok&=require(camera.distance()>fitted,"fit accounts for uniform scale");
    const float regularHeight=camera.orthographicHeight();camera.setViewport(1600,400);camera.fit();ok&=require(std::abs(camera.orthographicHeight()-regularHeight)<0.01f,"wide viewport preserves vertical fit");camera.setViewport(400,1600);camera.fit();ok&=require(camera.orthographicHeight()>regularHeight,"fit accounts for tall viewport aspect");
    for(int i=0;i<=int(PartViewerCamera::View::Bottom);++i){camera.setView(static_cast<PartViewerCamera::View>(i));ok&=require(!camera.viewMatrix().isIdentity(),"standard view produces view matrix");}

    PartViewerState state;const quint64 first=state.beginNewModelLoad();state.setScalePercent(200);const quint64 reload=state.beginReload();ok&=require(!state.accepts(first)&&state.accepts(reload)&&state.scalePercent()==200,"reload generation preserves scale and rejects stale result");const quint64 candidate=state.beginNewModelLoad();ok&=require(state.accepts(candidate)&&state.scalePercent()==100,"candidate load resets scale");state.setScalePercent(100.5);ok&=require(std::abs(state.scalePercent()-100.5)<0.0001,"fractional scale retained");state.resetScale();ok&=require(state.scalePercent()==100,"scale reset exact");
    const QStringList modes=partViewerRenderModeLabels();ok&=require(modes==QStringList({"Solid","Solid + Edges","Wireframe"}),"render combo exposes exactly three modes");
    const auto solid=partViewerRenderPasses(PartViewerRenderMode::Solid),edges=partViewerRenderPasses(PartViewerRenderMode::SolidEdges),wire=partViewerRenderPasses(PartViewerRenderMode::Wireframe);
    ok&=require(solid.visibleFaces&&!solid.ldrawEdges&&!solid.triangleEdges,"solid pass mapping");
    ok&=require(edges.visibleFaces&&edges.ldrawEdges&&!edges.triangleEdges&&!edges.depthPrepass,"solid plus edges pass mapping");
    ok&=require(!wire.visibleFaces&&wire.ldrawEdges&&wire.triangleEdges&&wire.depthPrepass,"wireframe pass mapping");
    ok&=require(LDrawColorResolver::faceColor("16")!=LDrawColorResolver::faceColor("4"),"inherited and standard colors resolve");ok&=require(LDrawColorResolver::faceColor("0x2FF0000").x()>0.99f,"direct color resolves");
    const QVector4D inspection=LDrawColorResolver::wireframeColor(),solidEdge=LDrawColorResolver::edgeColor("16");
    ok&=require(inspection.x()>0.70f&&inspection.y()>0.70f&&inspection.z()>0.70f&&inspection.w()==1.0f,"wireframe uses an opaque light-neutral inspection color");
    ok&=require((inspection.x()+inspection.y()+inspection.z())>(solidEdge.x()+solidEdge.y()+solidEdge.z())+1.5f,"wireframe color has substantially more luminance than solid edge color");
    return ok?0:1;
}

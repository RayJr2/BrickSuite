#include "../src/services/geometry/ConditionalEdgeVisibility.h"
#include "../src/services/geometry/LDrawColorResolver.h"
#include "../src/services/geometry/PartViewerCamera.h"
#include "../src/services/geometry/PartViewerState.h"

#include <QCoreApplication>
#include <QColor>
#include <QDebug>
#include <cmath>

namespace { bool require(bool value,const char*message){if(!value)qCritical("%s",message);return value;} }

namespace { bool near(const QVector3D&a,const QVector3D&b){return (a-b).length()<0.0001f;} }

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
    const float fitted=camera.distance();ok&=require(fitted>0&&camera.target()==QVector3D(),"fit establishes positive distance at bounds center");camera.orbit({400,300},{500,250});const QVector3D beforePan=camera.target();camera.pan(5,-3);ok&=require(camera.target()!=beforePan,"pan updates target");camera.zoom(1);ok&=require(camera.distance()<fitted,"zoom changes distance");
    const float perspectiveDistance=camera.distance();camera.setProjection(PartViewerCamera::Projection::Orthographic);ok&=require(camera.orthographicHeight()>0,"projection switch preserves framing");camera.setProjection(PartViewerCamera::Projection::Perspective);ok&=require(std::abs(camera.distance()-perspectiveDistance)<0.01f,"projection round trip preserves framing");camera.setProjection(PartViewerCamera::Projection::Orthographic);camera.setScale(2);camera.fit();ok&=require(camera.distance()>fitted,"fit accounts for uniform scale");
    const float regularHeight=camera.orthographicHeight();camera.setViewport(1600,400);camera.fit();ok&=require(std::abs(camera.orthographicHeight()-regularHeight)<0.01f,"wide viewport preserves vertical fit");camera.setViewport(400,1600);camera.fit();ok&=require(camera.orthographicHeight()>regularHeight,"fit accounts for tall viewport aspect");
    for(int i=0;i<=int(PartViewerCamera::View::Bottom);++i){camera.setView(static_cast<PartViewerCamera::View>(i));ok&=require(!camera.viewMatrix().isIdentity(),"standard view produces view matrix");}
    camera.setView(PartViewerCamera::View::Front);const QQuaternion initial=camera.orientation();
    for(int i=0;i<24;++i)camera.orbit({400,300},{400,210});
    ok&=require(std::abs(camera.orientation().length()-1.0f)<0.0001f,"repeated arcball composition remains normalized");
    ok&=require(camera.orientation()!=initial,"arcball rotates without an Euler pitch wall");
    camera.setView(PartViewerCamera::View::Top);const QQuaternion top=camera.orientation();camera.setView(PartViewerCamera::View::Bottom);ok&=require(camera.orientation()!=top,"top and bottom are distinct deterministic orientations");
    camera.orbit({400,300},{650,300});camera.setView(PartViewerCamera::View::Front);ok&=require(camera.orientation()==initial,"standard view replaces arbitrary arcball orientation");
    camera.setView(PartViewerCamera::View::Isometric);ok&=require(near(camera.viewDirection(),QVector3D(1,-1,1).normalized()),"isometric uses viewer Z-up direction");
    camera.setView(PartViewerCamera::View::Front);ok&=require(near(camera.viewDirection(),{0,0,-1})&&near(camera.viewUpDirection(),{0,-1,0}),"front uses viewer -Y with Z up");
    camera.setView(PartViewerCamera::View::Back);ok&=require(near(camera.viewDirection(),{0,0,1})&&near(camera.viewUpDirection(),{0,-1,0}),"back uses viewer +Y with Z up");
    camera.setView(PartViewerCamera::View::Left);ok&=require(near(camera.viewDirection(),{-1,0,0})&&near(camera.viewUpDirection(),{0,-1,0}),"left uses viewer -X with Z up");
    camera.setView(PartViewerCamera::View::Right);ok&=require(near(camera.viewDirection(),{1,0,0})&&near(camera.viewUpDirection(),{0,-1,0}),"right uses viewer +X with Z up");
    camera.setView(PartViewerCamera::View::Top);ok&=require(near(camera.viewDirection(),{0,-1,0})&&near(camera.viewUpDirection(),{0,0,-1}),"top looks down viewer Z");
    camera.setView(PartViewerCamera::View::Bottom);ok&=require(near(camera.viewDirection(),{0,1,0})&&near(camera.viewUpDirection(),{0,0,1}),"bottom looks up viewer Z");

    PartViewerState state;const quint64 first=state.beginNewModelLoad();state.setScalePercent(200);state.printOrientation().rotate(PrintOrientation::Rotation::XPositive);const quint64 reload=state.beginReload();ok&=require(!state.accepts(first)&&state.accepts(reload)&&state.scalePercent()==200&&!state.printOrientation().isIdentity(),"reload generation preserves scale and print orientation while rejecting stale result");const quint64 candidate=state.beginNewModelLoad();ok&=require(state.accepts(candidate)&&state.scalePercent()==100&&state.printOrientation().isIdentity(),"candidate load resets scale and print orientation");state.setScalePercent(100.5);ok&=require(std::abs(state.scalePercent()-100.5)<0.0001,"fractional scale retained");state.resetScale();ok&=require(state.scalePercent()==100,"scale reset exact");
    const auto inverse=[](PrintOrientation::Rotation positive,PrintOrientation::Rotation negative){PrintOrientation orientation;orientation.rotate(positive);orientation.rotate(negative);return orientation.isIdentity();};
    ok&=require(inverse(PrintOrientation::Rotation::XPositive,PrintOrientation::Rotation::XNegative),"X +90 and X -90 cancel");
    ok&=require(inverse(PrintOrientation::Rotation::YPositive,PrintOrientation::Rotation::YNegative),"Y +90 and Y -90 cancel");
    ok&=require(inverse(PrintOrientation::Rotation::ZPositive,PrintOrientation::Rotation::ZNegative),"Z +90 and Z -90 cancel");
    PrintOrientation cumulative;cumulative.rotate(PrintOrientation::Rotation::XPositive);cumulative.rotate(PrintOrientation::Rotation::YPositive);const auto cumulativePoint=cumulative.map({1,2,3});ok&=require(std::abs(cumulativePoint.x-2)<0.0001&&std::abs(cumulativePoint.y+3)<0.0001&&std::abs(cumulativePoint.z+1)<0.0001,"cumulative rotations use deterministic build-axis order");
    const QQuaternion cameraBeforeOrientation=camera.orientation();cumulative.rotate(PrintOrientation::Rotation::ZNegative);ok&=require(camera.orientation()==cameraBeforeOrientation,"print orientation remains independent of camera state");cumulative.reset();ok&=require(cumulative.isIdentity()&&cumulative.summary()==QStringLiteral("Nominal"),"Reset Orientation restores nominal state");
    const QStringList modes=partViewerRenderModeLabels();ok&=require(modes==QStringList({"Solid","Solid + Edges","Wireframe"}),"render combo exposes exactly three modes");
    const auto solid=partViewerRenderPasses(PartViewerRenderMode::Solid),edges=partViewerRenderPasses(PartViewerRenderMode::SolidEdges),wire=partViewerRenderPasses(PartViewerRenderMode::Wireframe);
    ok&=require(solid.visibleFaces&&!solid.ldrawEdges&&!solid.triangleEdges,"solid pass mapping");
    ok&=require(edges.visibleFaces&&edges.ldrawEdges&&!edges.triangleEdges&&!edges.depthPrepass,"solid plus edges pass mapping");
    ok&=require(!wire.visibleFaces&&wire.ldrawEdges&&wire.triangleEdges&&wire.depthPrepass,"wireframe pass mapping");
    ok&=require(partViewerShowAxesDefault(),"orientation axes default on");
    ok&=require(PartViewerAxisConvention::x()==QVector3D(1,0,0)&&PartViewerAxisConvention::y()==QVector3D(0,0,1)&&PartViewerAxisConvention::z()==QVector3D(0,-1,0),"viewer axes match OBJ X=+X Y=+Z Z=-Y convention");
    ok&=require(LDrawColorResolver::faceColor("16")!=LDrawColorResolver::faceColor("4"),"inherited and standard colors resolve");ok&=require(LDrawColorResolver::faceColor("0x2FF0000").x()>0.99f,"direct color resolves");
    const QColor red("#ff0000"),black("#050505");ok&=require(LDrawColorResolver::faceColor("16",red).x()>0.99f,"model color applies to inherited color 16");
    ok&=require(LDrawColorResolver::faceColor("4",red)==LDrawColorResolver::faceColor("4"),"explicit LDraw color ignores model color");
    ok&=require(LDrawColorResolver::faceColor("0x200ff00",red)==LDrawColorResolver::faceColor("0x200ff00"),"direct LDraw color ignores model color");
    const QVector4D darkEdge=LDrawColorResolver::edgeColor("16",black);ok&=require(darkEdge.x()>0.7f,"dark model color receives a light edge treatment");
    const QVector4D inspection=LDrawColorResolver::wireframeColor(),solidEdge=LDrawColorResolver::edgeColor("16");
    ok&=require(inspection.x()>0.70f&&inspection.y()>0.70f&&inspection.z()>0.70f&&inspection.w()==1.0f,"wireframe uses an opaque light-neutral inspection color");
    ok&=require((inspection.x()+inspection.y()+inspection.z())>(solidEdge.x()+solidEdge.y()+solidEdge.z())+1.5f,"wireframe color has substantially more luminance than solid edge color");
    return ok?0:1;
}

#pragma once

#include <QStringList>
#include <QtGlobal>

enum class PartViewerRenderMode { Solid, SolidEdges, Wireframe };

struct PartViewerRenderPasses
{
    bool visibleFaces=false;
    bool triangleEdges=false;
    bool ldrawEdges=false;
    bool depthPrepass=false;
};

inline QStringList partViewerRenderModeLabels()
{ return {QStringLiteral("Solid"),QStringLiteral("Solid + Edges"),QStringLiteral("Wireframe")}; }

inline PartViewerRenderPasses partViewerRenderPasses(PartViewerRenderMode mode)
{
    switch(mode){
    case PartViewerRenderMode::Solid:return {true,false,false,false};
    case PartViewerRenderMode::SolidEdges:return {true,false,true,false};
    case PartViewerRenderMode::Wireframe:return {false,true,true,true};
    }
    return {};
}

class PartViewerState
{
public:
    quint64 beginNewModelLoad(){m_scalePercent=100.0;return ++m_generation;}
    quint64 beginReload(){return ++m_generation;}
    bool accepts(quint64 generation)const{return generation==m_generation;}
    void setScalePercent(double value){m_scalePercent=qBound(1.0,value,1000.0);}
    void resetScale(){m_scalePercent=100.0;}
    double scalePercent()const{return m_scalePercent;}
private:
    quint64 m_generation=0;
    double m_scalePercent=100.0;
};

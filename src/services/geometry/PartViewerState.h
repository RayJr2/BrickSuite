/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "PrintOrientation.h"

#include <QStringList>
#include <QtGlobal>
#include <QVector3D>

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

struct PartViewerAxisConvention
{
    static QVector3D x(){return {1,0,0};}   // LDraw +X
    static QVector3D y(){return {0,0,1};}   // LDraw +Z
    static QVector3D z(){return {0,-1,0};}  // LDraw -Y
};

inline bool partViewerShowAxesDefault(){return true;}

class PartViewerState
{
public:
    quint64 beginNewModelLoad(){m_scalePercent=100.0;m_printOrientation.reset();return ++m_generation;}
    quint64 beginReload(){return ++m_generation;}
    bool accepts(quint64 generation)const{return generation==m_generation;}
    void setScalePercent(double value){m_scalePercent=qBound(1.0,value,1000.0);}
    void resetScale(){m_scalePercent=100.0;}
    double scalePercent()const{return m_scalePercent;}
    PrintOrientation& printOrientation(){return m_printOrientation;}
    const PrintOrientation& printOrientation()const{return m_printOrientation;}
private:
    quint64 m_generation=0;
    double m_scalePercent=100.0;
    PrintOrientation m_printOrientation;
};

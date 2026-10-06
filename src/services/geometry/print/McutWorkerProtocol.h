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
#include "PrintMesh.h"
#include <QDataStream>
#include <cmath>

namespace PrintGeometry::McutWorkerProtocol {
constexpr quint32 Version=1;
constexpr std::size_t MaximumFaces=50000;
constexpr std::size_t MaximumOutputFaces=100000;
constexpr int DeadlineMilliseconds=30000;
constexpr qint64 MaximumWireBytes=16*1024*1024;
inline void writeMesh(QDataStream& stream,const PrintMesh& mesh)
{
    stream<<quint32(mesh.vertices.size())<<quint32(mesh.faces.size());
    for(const auto& vertex:mesh.vertices)stream<<vertex.x<<vertex.y<<vertex.z;
    for(const auto& face:mesh.faces)for(auto index:face)stream<<quint32(index);
}
inline bool readMesh(QDataStream& stream,PrintMesh& mesh,std::size_t maximumFaces)
{
    quint32 vertices=0,faces=0;stream>>vertices>>faces;
    if(!vertices||!faces||vertices>3*maximumFaces||faces>maximumFaces)return false;
    mesh.vertices.resize(vertices);mesh.faces.resize(faces);
    for(auto& vertex:mesh.vertices){
        stream>>vertex.x>>vertex.y>>vertex.z;
        if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z))return false;
    }
    for(auto& face:mesh.faces)for(auto& index:face){stream>>index;if(index>=vertices)return false;}
    return stream.status()==QDataStream::Ok;
}
}

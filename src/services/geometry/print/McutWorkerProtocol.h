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

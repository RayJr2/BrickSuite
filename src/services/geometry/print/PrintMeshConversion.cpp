#include "PrintMeshConversion.h"

#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace PrintGeometry {
namespace {
struct Key{long long x,y,z;bool operator<(const Key&o)const{return std::tie(x,y,z)<std::tie(o.x,o.y,o.z);}};
Point convert(const QVector3D&p){return {0.4*double(p.x()),0.4*double(p.z()),-0.4*double(p.y())};}
Key key(const Point&p){constexpr double t=PrintMeshConversion::SeamWeldMillimetres;return {std::llround(p.x/t),std::llround(p.y/t),std::llround(p.z/t)};}
}
PrintMesh PrintMeshConversion::fromPartMesh(const LDrawGeometry::PartMesh&source,std::vector<std::string>*operations)
{
    PrintMesh out;std::map<Key,std::uint32_t>vertices;std::set<Face>faces;std::size_t degenerate=0,duplicates=0;
    auto index=[&](const QVector3D&p){const Point converted=convert(p);const Key k=key(converted);auto it=vertices.find(k);if(it!=vertices.end())return it->second;const auto id=std::uint32_t(out.vertices.size());vertices.emplace(k,id);out.vertices.push_back(converted);return id;};
    for(const auto&t:source.triangles){Face f{index(t.a),index(t.b),index(t.c)};if(f[0]==f[1]||f[1]==f[2]||f[2]==f[0]){++degenerate;continue;}if(!faces.insert(f).second){++duplicates;continue;}out.faces.push_back(f);}
    if(operations){operations->push_back("canonical Z-up millimetre conversion");operations->push_back("0.0004 mm deterministic seam weld");if(degenerate)operations->push_back("removed "+std::to_string(degenerate)+" degenerate faces");if(duplicates)operations->push_back("removed "+std::to_string(duplicates)+" same-winding duplicate faces");}
    return out;
}
} // namespace PrintGeometry

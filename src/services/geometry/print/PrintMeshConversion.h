#pragma once

#include "../PartMesh.h"
#include "PrintMesh.h"

namespace PrintGeometry {

class PrintMeshConversion
{
public:
    static constexpr double LDrawMillimetres = 0.4;
    static constexpr double SeamWeldMillimetres = 0.0004;
    static PrintMesh fromPartMesh(const LDrawGeometry::PartMesh& source, std::vector<std::string>* operations = nullptr);
};

} // namespace PrintGeometry

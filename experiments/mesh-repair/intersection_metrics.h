#pragma once
#include "types.h"
struct IntersectionMetrics {std::size_t points=0,segments=0,areas=0,degenerate=0,absent=0;double longest=0;};
IntersectionMetrics measureIntersections(const Mesh&,const std::vector<std::pair<Mesh::Face_index,Mesh::Face_index>>&);

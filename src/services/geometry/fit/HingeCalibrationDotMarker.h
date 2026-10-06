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

#include "../print/PrintMesh.h"

#include <cmath>
#include <vector>

namespace PrintGeometry::HingeCalibrationDotMarker {
// Two columns and four rows fit a compact exterior pad. A 1.04 mm diameter
// gives the 0.40 mm nozzle more than two extrusion widths per dot; the 0.60 mm
// relief provides three nominal 0.20 mm layers. The 0.56 mm minimum gap keeps
// neighboring dots individually countable after slicing.
inline PrintMesh dot(double x,double z,double padTopY) {
    constexpr unsigned Sides=12;
    constexpr double Radius=.52;
    constexpr double BottomOverlap=.12;
    constexpr double Relief=.60;
    PrintMesh mesh;
    for(unsigned ring=0;ring<2;++ring)for(unsigned i=0;i<Sides;++i) {
        const double angle=2.0*3.14159265358979323846*i/Sides;
        mesh.vertices.push_back({x+Radius*std::cos(angle),
            padTopY+(ring?Relief:-BottomOverlap),z+Radius*std::sin(angle)});
    }
    for(unsigned i=1;i<Sides-1;++i) {
        mesh.faces.push_back({0,i,i+1});
        mesh.faces.push_back({Sides,Sides+i+1,Sides+i});
    }
    for(unsigned i=0;i<Sides;++i) {
        const unsigned next=(i+1)%Sides;
        mesh.faces.push_back({i,Sides+i,Sides+next});
        mesh.faces.push_back({i,Sides+next,next});
    }
    return mesh;
}

inline std::vector<PrintMesh> dots(int candidate,double centerX,double centerZ,double padTopY) {
    std::vector<PrintMesh> result;
    if(candidate<1||candidate>7)return result;
    result.reserve(candidate);
    for(int i=0;i<candidate;++i) {
        const double x=centerX+(i%2==0?-.82:.82);
        const double z=centerZ+2.4-1.6*(i/2);
        result.push_back(dot(x,z,padTopY));
    }
    return result;
}
}

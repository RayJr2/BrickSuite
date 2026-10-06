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

#include "../src/services/geometry/print/PreparedMesh.h"
#include "../src/services/geometry/print/LDrawPrintPreparationProfile.h"
#include <QCoreApplication>
#include <QTextStream>
using namespace PrintGeometry;
int main(int argc,char**argv){QCoreApplication app(argc,argv);PreparedMesh prepared;prepared.mesh.vertices={{0,0,0},{1,0,0},{0,1,0}};prepared.mesh.faces={{0,1,2}};prepared.ldrawIdentity="fixture";prepared.preparationProfileVersion=LDrawPrintPreparationProfile::Version;prepared.sourceTriangleCount=1;prepared.preparedTriangleCount=1;prepared.dependencyFingerprint.dependencies.push_back({"parts/fixture.dat",42,{}});const auto copy=prepared;bool ok=copy.mesh.vertices.size()==3&&copy.ldrawIdentity=="fixture"&&copy.preparationProfileVersion==LDrawPrintPreparationProfile::Version&&copy.dependencyFingerprint.dependencies.front().relativePath=="parts/fixture.dat";if(!ok)QTextStream(stderr)<<"FAIL: PreparedMesh value semantics/provenance"<<Qt::endl;return ok?0:1;}

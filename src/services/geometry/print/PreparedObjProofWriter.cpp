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

#include "PreparedObjProofWriter.h"
#include <QSaveFile>
#include <QTextStream>
namespace PrintGeometry { bool PreparedObjProofWriter::write(const PrintMesh&m,const QString&path,QString*error){QSaveFile f(path);if(!f.open(QIODevice::WriteOnly|QIODevice::Text)){if(error)*error=f.errorString();return false;}QTextStream s(&f);s.setRealNumberNotation(QTextStream::FixedNotation);s.setRealNumberPrecision(9);s<<"# BrickSuite developer Prepared OBJ\n# Z-up millimetres; canonical nominal 100% scale\n";for(auto&p:m.vertices)s<<"v "<<p.x<<' '<<p.y<<' '<<p.z<<'\n';for(auto&t:m.faces)s<<"f "<<t[0]+1<<' '<<t[1]+1<<' '<<t[2]+1<<'\n';if(!f.commit()){if(error)*error=f.errorString();return false;}return true;} }

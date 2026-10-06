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

#include "CollectionCsvWriter.h"
#include <QFile>
#include <QTextStream>
namespace {QString quoted(QString v){v.replace('"',QStringLiteral("\"\""));return QStringLiteral("\"%1\"").arg(v);}}
CollectionCsvWriter::Result CollectionCsvWriter::generate(const CollectionExportProjection&p){if(p.headers.isEmpty())return{false,QStringLiteral("Select at least one export field."),{}};QString text(QChar(0xFEFF));QTextStream out(&text);QStringList h;for(const auto&v:p.headers)h<<quoted(v);out<<h.join(',')<<"\r\n";for(const auto&row:p.rows){QStringList values;for(const auto&v:row)values<<quoted(v);out<<values.join(',')<<"\r\n";}return{true,QStringLiteral("Prepared %1 Collection record(s).").arg(p.rows.size()),text};}
CollectionCsvWriter::Result CollectionCsvWriter::write(const QString&name,const CollectionExportProjection&p){auto r=generate(p);if(!r.success)return r;QFile f(name);if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate))return{false,QStringLiteral("Unable to write the selected CSV file."),{}};if(f.write(r.csv.toUtf8())<0)return{false,QStringLiteral("Unable to write the selected CSV file."),{}};r.message=QStringLiteral("Exported %1 Collection record(s).").arg(p.rows.size());return r;}

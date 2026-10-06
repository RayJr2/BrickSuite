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

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

struct CollectionExportRow {
    qint64 collectionItemId=0;
    QString type,reference,name,nickname,state,condition,completeness,location,source;
    bool allowPartsSource=false;
    QString notes,sourceBuildReference,sourceBuildName,createdUtc,modifiedUtc;
};

struct CollectionExportFieldDescriptor {
    QString id,label,header;
    bool defaultEnabled=false;
};

struct CollectionExportConfiguration {
    QStringList fieldOrder;
    QSet<QString> enabledFields;
};

struct CollectionExportProjection {
    QList<CollectionExportFieldDescriptor> fields;
    QStringList headers;
    QList<QStringList> rows;
};

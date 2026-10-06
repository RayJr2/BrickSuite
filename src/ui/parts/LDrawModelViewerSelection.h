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

#include <QDir>
#include <QFileInfo>
#include <QStringList>

// Chooses a default for presentation only. The authoritative candidate list
// and the user's later combo-box selection remain unchanged.
struct LDrawModelViewerSelection
{
    static int initialIndex(const QString& requestedPartNumber,const QStringList& candidates,
                            const QString& libraryRoot)
    {
        if(candidates.isEmpty())return -1;
        const auto modelId=[](QString value){
            value=value.trimmed();
            if(value.endsWith(QStringLiteral(".dat"),Qt::CaseInsensitive))value.chop(4);
            return value;
        };
        const QDir parts(QDir(libraryRoot).filePath(QStringLiteral("parts")));
        QStringList installedNames;
        bool listedNames=false;
        const auto installed=[&](const QString& candidate){
            const QString id=modelId(candidate);
            if(id.isEmpty()||id==QStringLiteral(".")||id==QStringLiteral("..")||
               id.contains('/')||id.contains('\\'))return false;
            const QString fileName=id+QStringLiteral(".dat");
            if(QFileInfo::exists(parts.filePath(fileName)))return true;
            if(!listedNames){installedNames=parts.entryList(QDir::Files);listedNames=true;}
            for(const auto&name:installedNames)
                if(name.compare(fileName,Qt::CaseInsensitive)==0)return true;
            return false;
        };
        const QString requested=modelId(requestedPartNumber);
        for(int i=0;i<candidates.size();++i)
            if(modelId(candidates[i]).compare(requested,Qt::CaseInsensitive)==0&&installed(candidates[i]))
                return i;
        for(int i=0;i<candidates.size();++i)if(installed(candidates[i]))return i;
        return 0;
    }
};

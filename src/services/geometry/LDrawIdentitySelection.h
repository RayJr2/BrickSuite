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
#include "../../models/ExternalPartIdentifier.h"
#include <QList>
#include <QStringList>
struct LDrawIdentitySelection
{
    static QStringList exactCandidates(const QList<ExternalPartIdentifier>& rows)
    {
        QStringList result;
        for(const auto& row:rows)
            if(row.isActive && row.provider.compare(QStringLiteral("LDraw"),Qt::CaseInsensitive)==0
               && !row.externalId.trimmed().isEmpty()) result<<row.externalId.trimmed();
        result.removeDuplicates(); result.sort(Qt::CaseInsensitive); return result;
    }
};

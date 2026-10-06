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

#include "../../api/rebrickable/RebrickableService.h"

#include <QObject>

class RebrickableMinifigPartsService : public QObject
{
    Q_OBJECT
public:
    struct Result
    {
        bool success = false;
        QString figNumber;
        int compositionRows = 0;
        int requiredPieces = 0;
        int sparePieces = 0;
        QString message;
    };

    explicit RebrickableMinifigPartsService(QObject* parent = nullptr);
    bool isBusy() const;
    void retrieveAndReplace(int minifigCatalogId,
                            const QString& figNumber,
                            const QString& apiKey);

signals:
    void finished(const RebrickableMinifigPartsService::Result& result);

private:
    RebrickableService* m_api = nullptr;
    bool m_busy = false;
    int m_minifigCatalogId = 0;
    QString m_figNumber;
};

Q_DECLARE_METATYPE(RebrickableMinifigPartsService::Result)

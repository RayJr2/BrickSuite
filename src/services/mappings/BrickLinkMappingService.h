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
#include <QString>

class BrickLinkMappingService : public QObject
{
    Q_OBJECT

public:
    struct ColorRefreshResult
    {
        bool success = false;
        QString message;

        int rebrickableColors = 0;
        int brickSuiteColors = 0;
        int matchedBrickSuiteColors = 0;

        int mapped = 0;
        int unsupported = 0;
        int unknown = 0;
    };

    explicit BrickLinkMappingService(QObject* parent = nullptr);

    void refreshColorMappings(const QString& rebrickableApiKey);

    void ensureColorMappings(const QString& rebrickableApiKey);

    bool storePartExternalIds(
        int partId,
        const QHash<QString, QStringList>& externalIds) const;

signals:
    void colorMappingsRefreshed(
        const BrickLinkMappingService::ColorRefreshResult& result);

private:
    void applyCatalogColors(
        const RebrickableService::CatalogColorsResult& apiResult);

    RebrickableService* m_rebrickableService = nullptr;
};

Q_DECLARE_METATYPE(BrickLinkMappingService::ColorRefreshResult)

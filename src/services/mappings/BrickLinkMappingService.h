/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
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

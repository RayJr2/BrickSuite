/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "ApiConnectionStatus.h"
#include "ApiProvider.h"

#include <QObject>

class ApiProviderStatusRegistry : public QObject
{
    Q_OBJECT
public:
    static ApiProviderStatusRegistry& instance();

    ApiConnectionStatus status(ApiProvider provider) const;
    void setStatus(ApiProvider provider, ApiConnectionStatus status);

    bool isConnected(ApiProvider provider) const;

signals:
    void statusChanged(ApiProvider provider, ApiConnectionStatus status);

private:
    ApiProviderStatusRegistry() = default;

    ApiProviderStatusRegistry(const ApiProviderStatusRegistry&) = delete;
    ApiProviderStatusRegistry& operator=(const ApiProviderStatusRegistry&) = delete;

    ApiConnectionStatus m_rebrickableStatus = ApiConnectionStatus::Unknown;
    ApiConnectionStatus m_brickLinkStatus = ApiConnectionStatus::Unknown;
    ApiConnectionStatus m_bricksetStatus = ApiConnectionStatus::Unknown;
};

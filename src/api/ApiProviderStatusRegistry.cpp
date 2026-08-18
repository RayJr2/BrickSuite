/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "ApiProviderStatusRegistry.h"

ApiProviderStatusRegistry& ApiProviderStatusRegistry::instance()
{
    static ApiProviderStatusRegistry registry;
    return registry;
}

ApiConnectionStatus ApiProviderStatusRegistry::status(ApiProvider provider) const
{
    switch (provider) {
    case ApiProvider::Rebrickable:
        return m_rebrickableStatus;
    case ApiProvider::BrickLink:
        return m_brickLinkStatus;
    case ApiProvider::Brickset:
        return m_bricksetStatus;
    }

    return ApiConnectionStatus::Unknown;
}

void ApiProviderStatusRegistry::setStatus(ApiProvider provider,
                                          ApiConnectionStatus status)
{
    switch (provider) {
    case ApiProvider::Rebrickable:
        m_rebrickableStatus = status;
        break;
    case ApiProvider::BrickLink:
        m_brickLinkStatus = status;
        break;
    case ApiProvider::Brickset:
        m_bricksetStatus = status;
        break;
    }
}

bool ApiProviderStatusRegistry::isConnected(ApiProvider provider) const
{
    return status(provider) == ApiConnectionStatus::Connected;
}

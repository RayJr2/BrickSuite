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
    if (this->status(provider) == status)
        return;

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

    emit statusChanged(provider, status);
}

bool ApiProviderStatusRegistry::isConnected(ApiProvider provider) const
{
    return status(provider) == ApiConnectionStatus::Connected;
}

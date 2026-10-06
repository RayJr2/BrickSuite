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

#include <QString>

namespace AppConstants {

inline constexpr char SupportUrl[] = "https://www.paypal.com/ncp/payment/WB8RKBVN6DTYW";

inline QString name()
{
    return QString::fromLatin1(APP_NAME);
}

inline QString company()
{
    return QString::fromLatin1(APP_COMPANY);
}

inline QString OrganizationName()
{
    return QString::fromLatin1(APP_ORGANIZATION_NAME);
}

inline QString domain()
{
    return QString::fromLatin1(APP_DOMAIN);
}

inline QString copyrightYear()
{
    return QString::fromLatin1(APP_COPYRIGHT_YEAR);
}

inline QString release()
{
    return QString::fromLatin1(APP_RELEASE);
}

inline QString releaseDate()
{
    return QString::fromLatin1(APP_RELEASE_DATE);
}

inline QString updateManifestUrl()
{
    return QString::fromLatin1(APP_UPDATE_MANIFEST_URL);
}

} // namespace AppConstants

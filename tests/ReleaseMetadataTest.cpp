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

#include "../src/core/AppVersion.h"
#include "../src/core/AppConstants.h"
#include <QCoreApplication>
#include <cstdio>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    const auto check = [&](bool condition, const char* message) {
        if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); ok = false; }
    };
    check(AppVersion::version() == QStringLiteral("0.4.1"), "v0.4.1 compiled version");
    check(AppConstants::release() == QStringLiteral("Release"), "production release channel");
    check(AppConstants::releaseDate() == QStringLiteral("2026-11-06"), "authoritative release date");
    check(AppConstants::name() == QStringLiteral("BrickSuite"), "product identity");
    check(AppConstants::company() == QStringLiteral("RF StateSide, LLC"), "publisher attribution");
    check(AppConstants::updateManifestUrl() == QStringLiteral(
        "https://raw.githubusercontent.com/RayJr2/BrickSuite/master/deployment/update/update.json"),
        "released Windows update endpoint preserved");
    check(QString::fromLatin1(AppConstants::SupportPayPalFallbackUrl) == QStringLiteral(
        "https://www.paypal.com/ncp/payment/WB8RKBVN6DTYW"), "central support address unchanged");
    check(QString::fromLatin1(AppConstants::SupportCheckoutUrl) == QStringLiteral(
        "https://rfstateside.com/support/checkout/bricksuite"), "central RFSS checkout");
    check(QString::fromLatin1(AppConstants::SupportAvailabilityUrl) == QStringLiteral(
        "https://rfstateside.com/support/checkout/bricksuite/availability"), "central availability endpoint");
    return ok ? 0 : 1;
}

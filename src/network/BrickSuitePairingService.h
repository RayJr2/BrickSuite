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

#include "PairedDeviceRegistry.h"

#include <QDateTime>
#include <functional>

class BrickSuitePairingService
{
public:
    struct Attempt { bool active = false; QString code; QDateTime expiresUtc; int failures = 0; QDateTime retryUtc; };
    struct Result { bool success = false; QString deviceId; QString credential; QString errorCode; QString error; };
    using Clock = std::function<QDateTime()>;
    using WriteCredential = std::function<bool(const QString&, const QString&, QString*)>;
    using RemoveCredential = std::function<bool(const QString&, QString*)>;

    explicit BrickSuitePairingService(PairedDeviceRegistry& registry,
                                      Clock clock = {}, WriteCredential write = {},
                                      RemoveCredential remove = {});
    Attempt start(QString* error = nullptr);
    void cancel();
    Attempt attempt() const;
    Result pair(const QString& code, const QString& friendlyName,
                const QString& clientVersion, const QString& platform);
    static QString normalizedCode(const QString& code, bool* valid = nullptr);
    static QString credentialReference(const QString& deviceId);
    static constexpr int ExpirationSeconds = 300;
    static constexpr int MaximumFailures = 5;

private:
    QDateTime now() const;
    void expireIfNeeded();
    PairedDeviceRegistry& m_registry;
    Clock m_clock;
    WriteCredential m_write;
    RemoveCredential m_remove;
    Attempt m_attempt;
};

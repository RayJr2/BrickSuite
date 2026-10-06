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

struct HostRequestContext
{
    enum class AuthenticationKind { LegacySharedToken, PairedDevice };

    QString sessionId;
    QString requestId;
    int protocolMinor = 0;
    AuthenticationKind authenticationKind = AuthenticationKind::LegacySharedToken;
    QString pairedDeviceId;

    QString fairnessOwner() const
    {
        return authenticationKind == AuthenticationKind::PairedDevice
            ? QStringLiteral("device:") + pairedDeviceId.trimmed().toLower()
            : QStringLiteral("session:") + sessionId;
    }

    QString mutationClientIdentity() const
    {
        if (authenticationKind == AuthenticationKind::LegacySharedToken)
            return QStringLiteral("LegacySharedToken");
        const QString deviceId = pairedDeviceId.trimmed().toLower();
        return deviceId.isEmpty() ? QString()
                                  : QStringLiteral("PairedDevice:") + deviceId;
    }

    static const HostRequestContext* current() { return s_current; }

private:
    friend class HostRequestContextScope;
    inline static thread_local const HostRequestContext* s_current = nullptr;
};

class HostRequestContextScope
{
public:
    explicit HostRequestContextScope(const HostRequestContext& context)
        : m_previous(HostRequestContext::s_current)
    { HostRequestContext::s_current = &context; }
    ~HostRequestContextScope() { HostRequestContext::s_current = m_previous; }
    HostRequestContextScope(const HostRequestContextScope&) = delete;
    HostRequestContextScope& operator=(const HostRequestContextScope&) = delete;
private:
    const HostRequestContext* m_previous;
};

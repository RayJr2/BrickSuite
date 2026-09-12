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
};

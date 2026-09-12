#pragma once

#include <QString>

enum class BrickSuiteConnectionState
{
    Disabled,
    Disconnected,
    Connecting,
    VerifyingHost,
    Authenticating,
    ConnectedAuthenticated,
    HostMaintenance,
    DeviceRevoked,
    AuthenticationFailed,
    HostIdentityMismatch,
    IncompatibleProtocol,
    Reconnecting,
    Error
};

struct BrickSuiteConnectionStatus
{
    BrickSuiteConnectionState state = BrickSuiteConnectionState::Disabled;
    QString message;
};

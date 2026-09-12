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

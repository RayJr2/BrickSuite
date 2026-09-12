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

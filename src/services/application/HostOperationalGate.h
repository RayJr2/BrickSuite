#pragma once

#include <atomic>

// Process-local authoritative guard for Host-local operational writes. Remote
// worker connections are governed by their admission cutoff and drain instead.
class HostOperationalGate
{
public:
    static bool localWritesAllowed() { return s_localWritesAllowed.load(); }
    static void setLocalWritesAllowed(bool allowed) { s_localWritesAllowed = allowed; }

private:
    inline static std::atomic_bool s_localWritesAllowed{true};
};


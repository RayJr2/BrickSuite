#pragma once

#include "BrickSuiteProtocol.h"

class HostOutboundPolicy
{
public:
    static constexpr qint64 HighWaterBytes = 2 * 1024 * 1024;
    static constexpr qint64 HardLimitBytes = 4 * 1024 * 1024;
    static constexpr int SustainedBackpressureMs = 10000;

    enum class Decision { Send, EnterBackpressure, Disconnect };

    static Decision evaluate(qint64 bufferedBytes, qsizetype messageBytes);
    static QByteArray serializeForSend(const BrickSuiteProtocol::Message& message,
                                       bool* replacedOversizedResponse,
                                       bool* rejectedOversizedEvent);
};

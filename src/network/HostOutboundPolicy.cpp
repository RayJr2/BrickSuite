#include "HostOutboundPolicy.h"

HostOutboundPolicy::Decision HostOutboundPolicy::evaluate(
    qint64 bufferedBytes, qsizetype messageBytes)
{
    if (bufferedBytes < 0 || messageBytes < 0
        || bufferedBytes > HardLimitBytes
        || messageBytes > HardLimitBytes - bufferedBytes)
        return Decision::Disconnect;
    return bufferedBytes >= HighWaterBytes
        ? Decision::EnterBackpressure : Decision::Send;
}

QByteArray HostOutboundPolicy::serializeForSend(
    const BrickSuiteProtocol::Message& message, bool* replacedOversizedResponse,
    bool* rejectedOversizedEvent)
{
    if (replacedOversizedResponse) *replacedOversizedResponse = false;
    if (rejectedOversizedEvent) *rejectedOversizedEvent = false;
    QByteArray serialized = BrickSuiteProtocol::serialize(message);
    if (serialized.size() <= BrickSuiteProtocol::MaximumMessageBytes) return serialized;
    if (message.type == BrickSuiteProtocol::MessageType::Event) {
        if (rejectedOversizedEvent) *rejectedOversizedEvent = true;
        return {};
    }
    const BrickSuiteProtocol::Message error = BrickSuiteProtocol::errorResponse(
        message, QStringLiteral("RESULT_TOO_LARGE"),
        QStringLiteral("The requested result is too large to transfer. Narrow the request and try again."));
    serialized = BrickSuiteProtocol::serialize(error);
    if (replacedOversizedResponse) *replacedOversizedResponse = true;
    return serialized.size() <= BrickSuiteProtocol::MaximumMessageBytes ? serialized : QByteArray();
}

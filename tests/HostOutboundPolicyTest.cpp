#include "../src/network/HostOutboundPolicy.h"

#include <QCoreApplication>
#include <cstdio>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    using Decision = HostOutboundPolicy::Decision;

    ok &= require(HostOutboundPolicy::evaluate(0, 1024) == Decision::Send,
                  "ordinary response sends");
    ok &= require(HostOutboundPolicy::evaluate(HostOutboundPolicy::HighWaterBytes, 1024)
                      == Decision::EnterBackpressure,
                  "high-water crossing starts bounded backpressure");
    ok &= require(HostOutboundPolicy::evaluate(HostOutboundPolicy::HardLimitBytes - 1024, 1024)
                      == Decision::EnterBackpressure,
                  "message ending exactly at hard limit is allowed");
    ok &= require(HostOutboundPolicy::evaluate(HostOutboundPolicy::HardLimitBytes - 1024, 1025)
                      == Decision::Disconnect,
                  "message exceeding hard limit disconnects only that socket");

    BrickSuiteProtocol::Message response = BrickSuiteProtocol::request(
        QStringLiteral("workspace.list"), {});
    response = BrickSuiteProtocol::response(response, {{QStringLiteral("data"), QString()}});
    const qsizetype baseSize = BrickSuiteProtocol::serialize(response).size();
    response.payload.insert(QStringLiteral("data"),
                            QString(int(BrickSuiteProtocol::MaximumMessageBytes - baseSize), QLatin1Char('x')));
    while (BrickSuiteProtocol::serialize(response).size() < BrickSuiteProtocol::MaximumMessageBytes)
        response.payload[QStringLiteral("data")] = response.payload.value(QStringLiteral("data")).toString()
            + QLatin1Char('x');
    while (BrickSuiteProtocol::serialize(response).size() > BrickSuiteProtocol::MaximumMessageBytes)
        response.payload[QStringLiteral("data")] = response.payload.value(QStringLiteral("data")).toString().chopped(1);

    bool replaced = false, rejectedEvent = false;
    QByteArray bytes = HostOutboundPolicy::serializeForSend(response, &replaced, &rejectedEvent);
    ok &= require(bytes.size() == BrickSuiteProtocol::MaximumMessageBytes
                      && !replaced && !rejectedEvent,
                  "response at exact UTF-8 wire limit sends unchanged");
    response.payload[QStringLiteral("data")] = response.payload.value(QStringLiteral("data")).toString()
        + QStringLiteral("é");
    bytes = HostOutboundPolicy::serializeForSend(response, &replaced, &rejectedEvent);
    const auto parsed = BrickSuiteProtocol::parse(bytes);
    ok &= require(replaced && !rejectedEvent && parsed.valid
                      && parsed.message.error.code == QStringLiteral("RESULT_TOO_LARGE")
                      && !parsed.message.error.retryable,
                  "oversized UTF-8 response becomes a small structured error");

    BrickSuiteProtocol::Message event = BrickSuiteProtocol::event(
        QStringLiteral("operational.invalidation"),
        {{QStringLiteral("data"), QString(int(BrickSuiteProtocol::MaximumMessageBytes),
                                           QLatin1Char('x'))}});
    bytes = HostOutboundPolicy::serializeForSend(event, &replaced, &rejectedEvent);
    ok &= require(bytes.isEmpty() && rejectedEvent && !replaced,
                  "oversized event is rejected rather than truncated");
    return ok ? 0 : 1;
}

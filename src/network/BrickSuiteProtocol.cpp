#include "BrickSuiteProtocol.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QSet>
#include <QUuid>
#include <QRegularExpression>
#include <cmath>

namespace {

BrickSuiteProtocol::Error invalid(const QString& message)
{
    return {QStringLiteral("INVALID_REQUEST"), message, false};
}

bool hasOnly(const QJsonObject& object, const QSet<QString>& allowed)
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!allowed.contains(it.key()))
            return false;
    }
    return true;
}

bool withinDepth(const QJsonValue& value, int remaining)
{
    if (remaining < 0) return false;
    if (value.isArray()) {
        for (const QJsonValue& child : value.toArray())
            if (!withinDepth(child, remaining - 1)) return false;
    } else if (value.isObject()) {
        for (const QJsonValue& child : value.toObject())
            if (!withinDepth(child, remaining - 1)) return false;
    }
    return true;
}

} // namespace

namespace BrickSuiteProtocol {

QString typeName(MessageType type)
{
    switch (type) {
    case MessageType::Request: return QStringLiteral("request");
    case MessageType::Response: return QStringLiteral("response");
    case MessageType::Error: return QStringLiteral("error");
    case MessageType::Event: return QStringLiteral("event");
    }
    return {};
}

QByteArray serialize(const Message& message)
{
    QJsonObject object{{QStringLiteral("protocol"),
                        QJsonObject{{QStringLiteral("major"), message.protocolMajor},
                                    {QStringLiteral("minor"), message.protocolMinor}}},
                       {QStringLiteral("type"), typeName(message.type)},
                       {QStringLiteral("operation"), message.operation},
                       {QStringLiteral("payload"), message.payload}};
    if (message.type != MessageType::Event)
        object.insert(QStringLiteral("requestId"), message.requestId);
    if (message.type == MessageType::Response || message.type == MessageType::Error)
        object.insert(QStringLiteral("success"), message.type == MessageType::Response);
    if (message.type == MessageType::Error) {
        object.insert(QStringLiteral("error"),
                      QJsonObject{{QStringLiteral("code"), message.error.code},
                                  {QStringLiteral("message"), message.error.message},
                                  {QStringLiteral("retryable"), message.error.retryable}});
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

ParseResult parse(const QByteArray& utf8)
{
    ParseResult result;
    if (utf8.size() > MaximumMessageBytes) {
        result.error = {QStringLiteral("PAYLOAD_TOO_LARGE"),
                        QStringLiteral("The protocol message exceeds the allowed size."), false};
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(utf8, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.error = invalid(QStringLiteral("The protocol message is not a valid JSON object."));
        return result;
    }
    const QJsonObject object = document.object();
    if (!withinDepth(object, 16)) {
        result.error = invalid(QStringLiteral("The protocol message is nested too deeply."));
        return result;
    }
    if (!hasOnly(object, {QStringLiteral("protocol"), QStringLiteral("type"),
                          QStringLiteral("requestId"), QStringLiteral("operation"),
                          QStringLiteral("payload"), QStringLiteral("success"),
                          QStringLiteral("error")})) {
        result.error = invalid(QStringLiteral("The protocol message contains unsupported fields."));
        return result;
    }
    if (!object.value(QStringLiteral("protocol")).isObject()) {
        result.error = invalid(QStringLiteral("The protocol version is missing."));
        return result;
    }
    const QJsonObject protocol = object.value(QStringLiteral("protocol")).toObject();
    if (!hasOnly(protocol, {QStringLiteral("major"), QStringLiteral("minor")})
        || !protocol.value(QStringLiteral("major")).isDouble()
        || !protocol.value(QStringLiteral("minor")).isDouble()) {
        result.error = invalid(QStringLiteral("The protocol version is invalid."));
        return result;
    }
    Message message;
    const double majorValue = protocol.value(QStringLiteral("major")).toDouble(-1);
    const double minorValue = protocol.value(QStringLiteral("minor")).toDouble(-1);
    message.protocolMajor = static_cast<int>(majorValue);
    message.protocolMinor = static_cast<int>(minorValue);
    if (message.protocolMajor < 0 || message.protocolMinor < 0
        || majorValue != std::floor(majorValue) || minorValue != std::floor(minorValue)
        || message.protocolMajor > 65535 || message.protocolMinor > 65535) {
        result.error = invalid(QStringLiteral("The protocol version is invalid."));
        return result;
    }
    const QString type = object.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("request")) message.type = MessageType::Request;
    else if (type == QStringLiteral("response")) message.type = MessageType::Response;
    else if (type == QStringLiteral("error")) message.type = MessageType::Error;
    else if (type == QStringLiteral("event")) message.type = MessageType::Event;
    else {
        result.error = invalid(QStringLiteral("The protocol message type is unsupported."));
        return result;
    }
    if ((message.type == MessageType::Request
         && (object.contains(QStringLiteral("success")) || object.contains(QStringLiteral("error"))))
        || (message.type == MessageType::Response && object.contains(QStringLiteral("error")))
        || (message.type == MessageType::Error && !object.contains(QStringLiteral("error")))
        || (message.type == MessageType::Event
            && (object.contains(QStringLiteral("requestId"))
                || object.contains(QStringLiteral("success"))
                || object.contains(QStringLiteral("error"))))) {
        result.error = invalid(QStringLiteral("The protocol fields do not match the message type."));
        return result;
    }
    message.requestId = object.value(QStringLiteral("requestId")).toString();
    message.operation = object.value(QStringLiteral("operation")).toString();
    static const QRegularExpression requestIdPattern(QStringLiteral("^[A-Za-z0-9_-]+$"));
    static const QRegularExpression operationPattern(QStringLiteral("^[a-z][A-Za-z0-9.]*$"));
    if ((message.type != MessageType::Event
         && (message.requestId.isEmpty() || message.requestId.size() > MaximumRequestIdLength
             || !requestIdPattern.match(message.requestId).hasMatch()))
        || message.operation.isEmpty() || message.operation.size() > MaximumOperationLength
        || !operationPattern.match(message.operation).hasMatch()
        || !object.value(QStringLiteral("payload")).isObject()) {
        result.error = invalid(QStringLiteral("Required protocol fields are missing or invalid."));
        return result;
    }
    message.payload = object.value(QStringLiteral("payload")).toObject();
    if (message.type == MessageType::Response || message.type == MessageType::Error) {
        if (!object.value(QStringLiteral("success")).isBool()) {
            result.error = invalid(QStringLiteral("The response success field is invalid."));
            return result;
        }
        message.success = object.value(QStringLiteral("success")).toBool();
        if ((message.type == MessageType::Response && !message.success)
            || (message.type == MessageType::Error && message.success)) {
            result.error = invalid(QStringLiteral("The response success value is inconsistent."));
            return result;
        }
    }
    if (message.type == MessageType::Error) {
        if (!object.value(QStringLiteral("error")).isObject()) {
            result.error = invalid(QStringLiteral("The protocol error object is missing."));
            return result;
        }
        const QJsonObject error = object.value(QStringLiteral("error")).toObject();
        if (!hasOnly(error, {QStringLiteral("code"), QStringLiteral("message"),
                             QStringLiteral("retryable")})
            || !error.value(QStringLiteral("code")).isString()
            || !error.value(QStringLiteral("message")).isString()
            || !error.value(QStringLiteral("retryable")).isBool()) {
            result.error = invalid(QStringLiteral("The protocol error object is invalid."));
            return result;
        }
        message.error = {error.value(QStringLiteral("code")).toString(),
                         error.value(QStringLiteral("message")).toString(),
                         error.value(QStringLiteral("retryable")).toBool()};
        if (message.error.code.isEmpty() || message.error.message.isEmpty()) {
            result.error = invalid(QStringLiteral("The protocol error object is invalid."));
            return result;
        }
    }
    result.valid = true;
    result.message = message;
    return result;
}

Message request(const QString& operation, const QJsonObject& payload)
{
    Message message;
    message.requestId = newRequestId();
    message.operation = operation;
    message.payload = payload;
    return message;
}

Message response(const Message& requestMessage, const QJsonObject& payload)
{
    Message message;
    message.type = MessageType::Response;
    message.requestId = requestMessage.requestId;
    message.operation = requestMessage.operation;
    message.payload = payload;
    return message;
}

Message errorResponse(const Message& requestMessage, const QString& code,
                      const QString& messageText, bool retryable)
{
    Message message;
    message.type = MessageType::Error;
    message.requestId = requestMessage.requestId;
    message.operation = requestMessage.operation;
    message.success = false;
    message.error = {code, messageText, retryable};
    return message;
}

Message event(const QString& operation, const QJsonObject& payload)
{
    Message message;
    message.type = MessageType::Event;
    message.operation = operation;
    message.payload = payload;
    return message;
}

QString newRequestId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace BrickSuiteProtocol

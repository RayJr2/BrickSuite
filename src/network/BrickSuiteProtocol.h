#pragma once

#include <QJsonObject>
#include <QString>

namespace BrickSuiteProtocol {

constexpr int Major = 1;
constexpr int Minor = 1;
constexpr qsizetype MaximumMessageBytes = 1024 * 1024;
constexpr int MaximumRequestIdLength = 128;
constexpr int MaximumOperationLength = 128;
constexpr int MaximumOutstandingRequests = 16;
constexpr int RequestTimeoutMs = 30000;
constexpr int AuthenticationTimeoutMs = 30000;
constexpr int MaximumClients = 8;
constexpr int MaximumAuthenticationFailures = 3;

enum class MessageType { Request, Response, Error, Event };

struct Error
{
    QString code;
    QString message;
    bool retryable = false;
};

struct Message
{
    int protocolMajor = Major;
    int protocolMinor = Minor;
    MessageType type = MessageType::Request;
    QString requestId;
    QString operation;
    bool success = true;
    QJsonObject payload;
    Error error;
};

struct ParseResult
{
    bool valid = false;
    Message message;
    Error error;
};

QString typeName(MessageType type);
QByteArray serialize(const Message& message);
ParseResult parse(const QByteArray& utf8);
Message request(const QString& operation, const QJsonObject& payload = {});
Message response(const Message& request, const QJsonObject& payload = {});
Message errorResponse(const Message& request, const QString& code,
                      const QString& message, bool retryable = false);
Message event(const QString& operation, const QJsonObject& payload);
QString newRequestId();

} // namespace BrickSuiteProtocol

/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QJsonObject>
#include <QString>

namespace BrickSuiteProtocol {

constexpr int Major = 1;
constexpr int Minor = 5;
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

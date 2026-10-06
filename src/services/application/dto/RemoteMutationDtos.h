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
#include <QJsonValue>
#include <QByteArray>
#include <QString>

namespace RemoteMutationDto {

enum class Outcome { Success, DefinitiveFailure, Unknown };

struct Metadata {
    qint64 workspaceId = 0;
    QString mutationId;
    QJsonObject expected;
    QJsonObject mutation;
    QString dataEpoch;
};

struct RequestContext {
    QString operation;
    qint64 workspaceId = 0;
    QString mutationId;
    QString clientIdentity;
    int protocolMajor = 1;
    int protocolMinor = 2;
};

struct Error {
    QString code;
    QString message;
    bool retryable = false;
    Outcome outcome = Outcome::DefinitiveFailure;
    QJsonObject conflict;
    QString mutationId;
};

struct Result {
    QString mutationId;
    QString operation;
    bool replayed = false;
    QString committedUtc;
    QJsonObject authoritative;
};

QString newMutationId();
bool parseMetadata(const QJsonObject& payload, Metadata* result, Error* error = nullptr);
QJsonObject resultToJson(const Result& result);
bool resultFromJson(const QJsonObject& object, Result* result, Error* error = nullptr);
QByteArray canonicalJson(const QJsonValue& value);
QString requestHash(const QString& operation, const Metadata& metadata);

} // namespace RemoteMutationDto

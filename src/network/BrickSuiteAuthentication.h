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

#include <QByteArray>
#include <QString>

namespace BrickSuiteAuthentication {

QByteArray secureRandom(int byteCount, QString* error = nullptr);
QString generateAccessToken(QString* error = nullptr);
QByteArray authenticationInput(const QByteArray& challenge,
                               const QByteArray& clientNonce,
                               const QByteArray& sessionId,
                               int protocolMajor, int protocolMinor);
QByteArray hmacSha256(const QByteArray& secret, const QByteArray& message,
                     QString* error = nullptr);
bool constantTimeEquals(const QByteArray& left, const QByteArray& right);

} // namespace BrickSuiteAuthentication

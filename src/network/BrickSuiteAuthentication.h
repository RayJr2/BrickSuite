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

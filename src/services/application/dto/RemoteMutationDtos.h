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

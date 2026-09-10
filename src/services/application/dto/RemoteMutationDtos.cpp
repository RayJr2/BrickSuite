#include "RemoteMutationDtos.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace {
QJsonValue canonicalValue(const QJsonValue& value)
{
    if (value.isArray()) {
        QJsonArray result;
        for (const QJsonValue& child : value.toArray()) result.append(canonicalValue(child));
        return result;
    }
    if (value.isObject()) {
        const QJsonObject source = value.toObject();
        QStringList keys = source.keys();
        std::sort(keys.begin(), keys.end());
        QJsonObject result;
        for (const QString& key : keys) result.insert(key, canonicalValue(source.value(key)));
        return result;
    }
    return value;
}
}

namespace RemoteMutationDto {
QString newMutationId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

bool parseMetadata(const QJsonObject& payload, Metadata* result, Error* error)
{
    static const QRegularExpression uuid(QStringLiteral(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-5][0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}$"));
    const double workspace = payload.value(QStringLiteral("workspaceId")).toDouble(-1);
    const QString mutationId = payload.value(QStringLiteral("mutationId")).toString();
    bool fieldsValid = payload.size() == 4;
    for (auto it = payload.constBegin(); it != payload.constEnd(); ++it)
        fieldsValid = fieldsValid && (it.key() == QStringLiteral("workspaceId")
            || it.key() == QStringLiteral("mutationId")
            || it.key() == QStringLiteral("expected")
            || it.key() == QStringLiteral("mutation"));
    if (!result || !fieldsValid || workspace < 1 || workspace > 9007199254740991.0
        || workspace != std::floor(workspace) || !uuid.match(mutationId).hasMatch()
        || !payload.value(QStringLiteral("expected")).isObject()
        || !payload.value(QStringLiteral("mutation")).isObject()) {
        if (error) *error = {QStringLiteral("INVALID_ARGUMENT"),
            QStringLiteral("The mutation metadata is invalid."), false};
        return false;
    }
    result->workspaceId = static_cast<qint64>(workspace);
    result->mutationId = mutationId.toLower();
    result->expected = payload.value(QStringLiteral("expected")).toObject();
    result->mutation = payload.value(QStringLiteral("mutation")).toObject();
    return true;
}

QByteArray canonicalJson(const QJsonValue& value)
{
    const QJsonValue canonical = canonicalValue(value);
    return canonical.isArray()
        ? QJsonDocument(canonical.toArray()).toJson(QJsonDocument::Compact)
        : QJsonDocument(canonical.toObject()).toJson(QJsonDocument::Compact);
}

QString requestHash(const QString& operation, const Metadata& metadata)
{
    QJsonObject business{{QStringLiteral("operation"), operation},
                         {QStringLiteral("workspaceId"), metadata.workspaceId},
                         {QStringLiteral("expected"), metadata.expected},
                         {QStringLiteral("mutation"), metadata.mutation}};
    return QString::fromLatin1(QCryptographicHash::hash(canonicalJson(business),
                                                        QCryptographicHash::Sha256).toHex());
}

QJsonObject resultToJson(const Result& result)
{
    return {{QStringLiteral("mutationId"), result.mutationId},
            {QStringLiteral("operation"), result.operation},
            {QStringLiteral("replayed"), result.replayed},
            {QStringLiteral("committedUtc"), result.committedUtc},
            {QStringLiteral("authoritative"), result.authoritative}};
}

bool resultFromJson(const QJsonObject& object, Result* result, Error* error)
{
    if (!result || !object.value(QStringLiteral("authoritative")).isObject()) {
        if (error) *error = {QStringLiteral("INTERNAL_ERROR"),
            QStringLiteral("The Host returned an invalid mutation result."), false};
        return false;
    }
    result->mutationId = object.value(QStringLiteral("mutationId")).toString();
    result->operation = object.value(QStringLiteral("operation")).toString();
    result->replayed = object.value(QStringLiteral("replayed")).toBool();
    result->committedUtc = object.value(QStringLiteral("committedUtc")).toString();
    result->authoritative = object.value(QStringLiteral("authoritative")).toObject();
    return !result->mutationId.isEmpty() && !result->operation.isEmpty();
}
} // namespace RemoteMutationDto

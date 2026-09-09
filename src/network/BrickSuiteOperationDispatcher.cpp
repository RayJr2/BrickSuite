#include "BrickSuiteOperationDispatcher.h"

#include "../database/DatabaseSchema.h"

#include <QDateTime>
#include <QJsonArray>

BrickSuiteOperationDispatcher::BrickSuiteOperationDispatcher()
{
    registerOperation(QStringLiteral("system.capabilities"), true,
        [this](const QJsonObject&) {
        QJsonArray names;
        for (const QString& operation : operations()) names.append(operation);
        return QJsonObject{
            {QStringLiteral("brickSuiteVersion"), QStringLiteral(BRICKSUITE_VERSION)},
            {QStringLiteral("protocolMajor"), BrickSuiteProtocol::Major},
            {QStringLiteral("protocolMinor"), BrickSuiteProtocol::Minor},
            {QStringLiteral("schemaVersion"), DatabaseSchema::CurrentSchemaVersion},
            {QStringLiteral("maintenance"), false},
            {QStringLiteral("sharedBusinessDataAvailable"), false},
            {QStringLiteral("catalogStatusAvailable"), false},
            {QStringLiteral("operations"), names}};
    });
    registerOperation(QStringLiteral("system.ping"), true,
        [](const QJsonObject&) {
        return QJsonObject{{QStringLiteral("serverUtc"),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    });
    registerOperation(QStringLiteral("system.catalogStatus"), true,
        [](const QJsonObject&) {
        return QJsonObject{{QStringLiteral("supported"), false}};
    });
}

void BrickSuiteOperationDispatcher::registerOperation(
    const QString& name, bool authenticationRequired, Handler handler)
{
    m_operations.insert(name, {authenticationRequired, std::move(handler)});
}

BrickSuiteProtocol::Message BrickSuiteOperationDispatcher::dispatch(
    const BrickSuiteProtocol::Message& request, bool authenticated) const
{
    const auto it = m_operations.constFind(request.operation);
    if (it == m_operations.constEnd())
        return BrickSuiteProtocol::errorResponse(request, QStringLiteral("UNKNOWN_OPERATION"),
            QStringLiteral("The requested operation is not supported."));
    if (it->authenticationRequired && !authenticated)
        return BrickSuiteProtocol::errorResponse(request, QStringLiteral("AUTH_REQUIRED"),
            QStringLiteral("Authenticate before requesting this operation."));
    return BrickSuiteProtocol::response(request, it->handler(request.payload));
}

QStringList BrickSuiteOperationDispatcher::operations() const
{
    QStringList names = m_operations.keys();
    names.append(QStringLiteral("system.hello"));
    names.append(QStringLiteral("system.authenticate"));
    names.sort();
    return names;
}

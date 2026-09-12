#include "BrickSuiteOperationDispatcher.h"
#include "OperationalInvalidation.h"

#include "../database/DatabaseSchema.h"

#include <QDateTime>
#include <QJsonArray>

BrickSuiteOperationDispatcher::BrickSuiteOperationDispatcher()
{
    registerOperation(QStringLiteral("system.capabilities"), true,
        [this](const QJsonObject&) {
        QJsonArray names;
        for (const QString& operation : operations()) names.append(operation);
        QJsonArray capabilities;
        capabilities.append(OperationalInvalidation::Capability);
        const auto addCapability = [this, &capabilities](const QString& operation,
                                                         const QString& capability) {
            if (m_operations.contains(operation)) capabilities.append(capability);
        };
        addCapability(QStringLiteral("workspace.list"), QStringLiteral("workspace.read"));
        addCapability(QStringLiteral("storage.list"), QStringLiteral("storage.read"));
        addCapability(QStringLiteral("storage.get"), QStringLiteral("storage.get"));
        addCapability(QStringLiteral("storage.types.list"), QStringLiteral("storage.types.list"));
        addCapability(QStringLiteral("storage.add"), QStringLiteral("storage.add"));
        addCapability(QStringLiteral("storage.edit"), QStringLiteral("storage.edit"));
        addCapability(QStringLiteral("storage.setActive"), QStringLiteral("storage.setActive"));
        addCapability(QStringLiteral("inventory.search"), QStringLiteral("inventory.read"));
        addCapability(QStringLiteral("inventory.get"), QStringLiteral("inventory.detail.read"));
        addCapability(QStringLiteral("inventory.history"), QStringLiteral("inventory.history.read"));
        addCapability(QStringLiteral("inventory.lost.list"), QStringLiteral("inventory.lost.read"));
        addCapability(QStringLiteral("builds.list"), QStringLiteral("builds.read"));
        addCapability(QStringLiteral("builds.get"), QStringLiteral("builds.detail.read"));
        addCapability(QStringLiteral("builds.requirements"), QStringLiteral("builds.requirements.read"));
        addCapability(QStringLiteral("builds.missingParts"), QStringLiteral("builds.missingParts.read"));
        addCapability(QStringLiteral("builds.pulling"), QStringLiteral("builds.pulling.read"));
        addCapability(QStringLiteral("collection.search"), QStringLiteral("collection.read"));
        addCapability(QStringLiteral("partReference.customizations"),
                      QStringLiteral("partReference.customizations.read"));
        addCapability(QStringLiteral("partReference.customizations.add"),
                      QStringLiteral("partReference.customizations.add"));
        addCapability(QStringLiteral("partReference.customizations.remove"),
                      QStringLiteral("partReference.customizations.remove"));
        for (const QString& operation : {QStringLiteral("builds.add"),
                                         QStringLiteral("builds.edit"),
                                         QStringLiteral("builds.setActive"),
                                         QStringLiteral("builds.complete"),
                                         QStringLiteral("builds.cancel"),
                                         QStringLiteral("builds.disassemble"),
                                         QStringLiteral("builds.spare.store"),
                                         QStringLiteral("builds.requirements.add"),
                                         QStringLiteral("builds.requirements.edit"),
                                         QStringLiteral("builds.requirements.remove"),
                                         QStringLiteral("builds.allocations.set"),
                                         QStringLiteral("builds.allocateAvailable")})
            addCapability(operation, operation);
        const bool sharedReads = m_operations.contains(QStringLiteral("workspace.list"));
        QJsonObject result{
            {QStringLiteral("brickSuiteVersion"), QStringLiteral(BRICKSUITE_VERSION)},
            {QStringLiteral("protocolMajor"), BrickSuiteProtocol::Major},
            {QStringLiteral("protocolMinor"), BrickSuiteProtocol::Minor},
            {QStringLiteral("schemaVersion"), DatabaseSchema::CurrentSchemaVersion},
            {QStringLiteral("maintenance"), false},
            {QStringLiteral("sharedBusinessDataAvailable"), sharedReads},
            {QStringLiteral("catalogStatusAvailable"), false},
            {QStringLiteral("capabilities"), capabilities},
            {QStringLiteral("operations"), names}};
        if (!m_dataEpoch.isEmpty())
            result.insert(QStringLiteral("dataEpoch"), m_dataEpoch);
        return result;
    });
    registerOperation(QStringLiteral("system.ping"), true,
        [](const QJsonObject&) {
        return QJsonObject{{QStringLiteral("serverUtc"),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    });
    registerOperation(QStringLiteral("system.status"), true,
        [](const QJsonObject&) {
        return QJsonObject{{QStringLiteral("maintenance"), false}};
    });
    registerOperation(QStringLiteral("system.catalogStatus"), true,
        [](const QJsonObject&) {
        return QJsonObject{{QStringLiteral("supported"), false}};
    });
}

void BrickSuiteOperationDispatcher::registerOperation(
    const QString& name, bool authenticationRequired, Handler handler)
{
    m_operations.insert(name, {authenticationRequired, std::move(handler), {}, 0, {}});
}

void BrickSuiteOperationDispatcher::registerAsyncOperation(
    const QString& name, bool authenticationRequired, AsyncHandler handler,
    int minimumMinor, const QString& capability)
{
    m_operations.insert(name, {authenticationRequired, {}, std::move(handler),
                               minimumMinor, capability});
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
    if (!it->handler)
        return BrickSuiteProtocol::errorResponse(request, QStringLiteral("ASYNC_REQUIRED"),
            QStringLiteral("This operation completes asynchronously."), true);
    return BrickSuiteProtocol::response(request, it->handler(request.payload));
}

void BrickSuiteOperationDispatcher::dispatchAsync(
    const BrickSuiteProtocol::Message& request, bool authenticated,
    Completion completion) const
{
    const auto it = m_operations.constFind(request.operation);
    if (it == m_operations.constEnd()) {
        completion(BrickSuiteProtocol::errorResponse(request, QStringLiteral("UNKNOWN_OPERATION"),
            QStringLiteral("The requested operation is not supported.")));
        return;
    }
    if (it->authenticationRequired && !authenticated) {
        completion(BrickSuiteProtocol::errorResponse(request, QStringLiteral("AUTH_REQUIRED"),
            QStringLiteral("Authenticate before requesting this operation.")));
        return;
    }
    if (request.protocolMinor < it->minimumMinor) {
        completion(BrickSuiteProtocol::errorResponse(request, QStringLiteral("FORBIDDEN"),
            QStringLiteral("The negotiated protocol does not support this operation.")));
        return;
    }
    if (it->asyncHandler) {
        it->asyncHandler(request, std::move(completion));
        return;
    }
    completion(BrickSuiteProtocol::response(request, it->handler(request.payload)));
}

QStringList BrickSuiteOperationDispatcher::operations() const
{
    QStringList names = m_operations.keys();
    names.append(QStringLiteral("system.hello"));
    names.append(QStringLiteral("system.authenticate"));
    names.sort();
    return names;
}

QStringList BrickSuiteOperationDispatcher::operations(int negotiatedMinor) const
{
    QStringList names;
    for (auto it = m_operations.cbegin(); it != m_operations.cend(); ++it)
        if (it->minimumMinor <= negotiatedMinor) names.append(it.key());
    names.append(QStringLiteral("system.hello"));
    names.append(QStringLiteral("system.authenticate"));
    names.sort();
    return names;
}

QStringList BrickSuiteOperationDispatcher::capabilities(int negotiatedMinor) const
{
    QStringList result;
    for (auto it = m_operations.cbegin(); it != m_operations.cend(); ++it)
        if (it->minimumMinor <= negotiatedMinor && !it->capability.isEmpty())
            result.append(it->capability);
    result.removeDuplicates();
    result.sort();
    return result;
}

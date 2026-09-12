#include "PairedDeviceAdministrationService.h"

#include "BrickSuiteWebSocketServer.h"
#include "../services/CredentialStore.h"

#include <algorithm>

PairedDeviceAdministrationService::PairedDeviceAdministrationService(
    PairedDeviceRegistry& registry, BrickSuiteWebSocketServer& server,
    CredentialReader reader, CredentialRemover remover, QObject* parent)
    : QObject(parent), m_registry(registry), m_server(server)
    , m_reader(reader ? std::move(reader) : [](const QString& reference) {
        const auto result = CredentialStore::read(reference);
        return CredentialState{result.success, result.found, result.error};
    })
    , m_remover(remover ? std::move(remover)
                        : [](const QString& reference, QString* error) {
        return CredentialStore::remove(reference, error);
    })
{
    connect(&m_server, &BrickSuiteWebSocketServer::statusChanged,
            this, &PairedDeviceAdministrationService::devicesChanged);
}

QVector<PairedDeviceAdministrationService::DeviceInfo>
PairedDeviceAdministrationService::devices(QString* error) const
{
    if (error) error->clear();
    if (!m_registry.available()) {
        if (error) *error = m_registry.lastError();
        return {};
    }
    QVector<DeviceInfo> result;
    for (const auto& record : m_registry.devices()) {
        const CredentialState credential = m_reader(record.credentialReference);
        result.append({record, m_server.authenticatedDeviceSessionCount(record.deviceId),
                       credential.found, credential.success});
    }
    std::sort(result.begin(), result.end(), [](const DeviceInfo& left, const DeviceInfo& right) {
        const int name = QString::compare(left.record.friendlyName, right.record.friendlyName,
                                          Qt::CaseInsensitive);
        return name != 0 ? name < 0 : left.record.deviceId < right.record.deviceId;
    });
    return result;
}

PairedDeviceAdministrationService::Result
PairedDeviceAdministrationService::renameDevice(const QString& deviceId,
                                                  const QString& friendlyName)
{
    QString error;
    if (!m_registry.rename(deviceId, friendlyName, &error)) return {false, error};
    emit devicesChanged();
    return {true, {}};
}

PairedDeviceAdministrationService::Result
PairedDeviceAdministrationService::revokeDevice(const QString& deviceId)
{
    const auto record = m_registry.find(deviceId);
    if (!record) return {false, QStringLiteral("The paired device was not found.")};
    QString error;
    // Persist the authorization denial before touching the secret or live sessions. No event
    // processing occurs between these synchronous steps, so a new request cannot be admitted.
    if (!m_registry.deactivate(deviceId, &error)) return {false, error};
    m_server.disconnectAuthenticatedDevice(deviceId);
    if (!m_remover(record->credentialReference, &error)) {
        emit devicesChanged();
        return {false, QStringLiteral("The device was revoked and disconnected, but its protected credential could not be removed: %1").arg(error)};
    }
    if (!m_registry.remove(deviceId, &error)) {
        emit devicesChanged();
        return {false, QStringLiteral("The device was revoked, but its inactive record could not be removed: %1").arg(error)};
    }
    emit devicesChanged();
    return {true, {}};
}

PairedDeviceAdministrationService::Result
PairedDeviceAdministrationService::revokeAllDevices()
{
    const auto records = m_registry.devices();
    QString error;
    if (!m_registry.deactivateAll(&error)) return {false, error};
    m_server.disconnectAllAuthenticatedDevices();
    QStringList failures;
    for (const auto& record : records) {
        QString removeError;
        if (!m_remover(record.credentialReference, &removeError))
            failures.append(record.friendlyName + QStringLiteral(": ") + removeError);
    }
    if (!failures.isEmpty()) {
        emit devicesChanged();
        return {false, QStringLiteral("All devices were revoked and disconnected, but some protected credentials could not be removed: %1")
                           .arg(failures.join(QStringLiteral("; ")))};
    }
    if (!m_registry.removeAllInactive(&error)) {
        emit devicesChanged();
        return {false, QStringLiteral("All devices were revoked, but inactive records could not be removed: %1").arg(error)};
    }
    emit devicesChanged();
    return {true, {}};
}

int PairedDeviceAdministrationService::legacyAuthenticatedClientCount() const
{
    return m_server.legacyAuthenticatedClientCount();
}

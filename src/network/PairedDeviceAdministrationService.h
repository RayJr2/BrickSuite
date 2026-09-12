#pragma once

#include "PairedDeviceRegistry.h"

#include <QObject>
#include <functional>

class BrickSuiteWebSocketServer;

class PairedDeviceAdministrationService : public QObject
{
    Q_OBJECT
public:
    struct CredentialState { bool success = false; bool found = false; QString error; };
    struct DeviceInfo {
        PairedDeviceRecord record;
        int connectedSessions = 0;
        bool credentialAvailable = false;
        bool credentialCheckSucceeded = false;
    };
    struct Result { bool success = false; QString error; };
    using CredentialReader = std::function<CredentialState(const QString&)>;
    using CredentialRemover = std::function<bool(const QString&, QString*)>;

    PairedDeviceAdministrationService(PairedDeviceRegistry& registry,
                                      BrickSuiteWebSocketServer& server,
                                      CredentialReader reader = {},
                                      CredentialRemover remover = {},
                                      QObject* parent = nullptr);

    QVector<DeviceInfo> devices(QString* error = nullptr) const;
    Result renameDevice(const QString& deviceId, const QString& friendlyName);
    Result revokeDevice(const QString& deviceId);
    Result revokeAllDevices();
    int legacyAuthenticatedClientCount() const;

signals:
    void devicesChanged();

private:
    PairedDeviceRegistry& m_registry;
    BrickSuiteWebSocketServer& m_server;
    CredentialReader m_reader;
    CredentialRemover m_remover;
};

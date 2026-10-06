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

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

#include <QObject>
#include <QString>
#include <optional>

class RemoteSessionState : public QObject
{
    Q_OBJECT
public:
    enum class DataState { NeverLoaded, Current, StaleDisconnected };
    Q_ENUM(DataState)

    struct Snapshot {
        QString hostIdentity;
        quint64 sessionGeneration = 0;
        quint64 workspaceGeneration = 0;
        int workspaceId = 0;
    };

    explicit RemoteSessionState(QObject* parent = nullptr);

    QString hostIdentity() const;
    QString dataEpoch() const;
    bool dataEpochSupported() const;
    quint64 sessionGeneration() const;
    quint64 workspaceGeneration() const;
    int workspaceId() const;
    bool isAuthenticated() const;
    DataState dataState() const;
    Snapshot snapshot() const;
    bool accepts(const Snapshot& snapshot) const;
    bool acceptsHostGlobal(const Snapshot& snapshot) const;
    bool acceptsEvent(quint64 sessionGeneration,
                      const std::optional<qint64>& workspaceId) const;

public slots:
    void authenticated(const QString& verifiedFingerprint);
    void authenticatedWithEpoch(const QString& verifiedFingerprint,
                                const QString& dataEpoch, bool epochSupported);
    void disconnected();
    void setWorkspaceId(int workspaceId);
    void markCurrent();

signals:
    void authenticatedSessionEstablished(bool sameHost);
    void sameHostSessionRestored();
    void authenticatedSessionLost();
    void hostIdentityChanged();
    void workspaceGenerationChanged(quint64 generation, int workspaceId);
    void dataStateChanged(RemoteSessionState::DataState state);
    void operationalStateMustClear();
    void operationalStateShouldRefresh();
    void dataEpochChanged();

private:
    void setDataState(DataState state);
    void advanceWorkspaceGeneration(int workspaceId);

    QString m_hostIdentity;
    QString m_dataEpoch;
    bool m_dataEpochSupported = false;
    quint64 m_sessionGeneration = 0;
    quint64 m_workspaceGeneration = 0;
    int m_workspaceId = 0;
    bool m_authenticated = false;
    bool m_hadAuthenticatedSession = false;
    DataState m_dataState = DataState::NeverLoaded;
};

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

#include "DatabaseRestoreTransaction.h"

#include <QObject>

class AutomaticBackupService;
class BrickSuiteNetworkManager;
class QThread;

class DatabaseRestoreCoordinator : public QObject
{
    Q_OBJECT
public:
    DatabaseRestoreCoordinator(BrickSuiteNetworkManager& network,
                               AutomaticBackupService& automaticBackups,
                               QObject* parent = nullptr);
    ~DatabaseRestoreCoordinator() override;

    bool isRunning() const { return m_running; }
    void start(const QString& candidatePath);

signals:
    void phaseChanged(const QString& message);
    void localQuiesceRequired();
    void finished(const DatabaseRestoreTransaction::Result& result);

private:
    void runPreflight();
    void waitForAutomaticBackup();
    void enterMaintenance();
    void runReplacement();

    BrickSuiteNetworkManager& m_network;
    AutomaticBackupService& m_automaticBackups;
    QString m_candidatePath;
    QString m_stagedCandidatePath;
    bool m_running = false;
    bool m_destructiveStarted = false;
    QThread* m_worker = nullptr;
};

Q_DECLARE_METATYPE(DatabaseRestoreTransaction::Result)

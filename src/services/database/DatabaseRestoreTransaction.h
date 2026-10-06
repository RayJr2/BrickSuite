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

#include "DatabaseFileValidator.h"

#include <QString>
#include <functional>

class DatabaseRestoreTransaction
{
public:
    struct FaultInjection {
        bool failSafetyBackup = false;
        bool failReplacement = false;
        bool failInstalledValidation = false;
        bool failEpochCommit = false;
        bool failRollback = false;
    };
    using Progress = std::function<void(const QString&)>;

    enum class Failure {
        None,
        Candidate,
        SafetyBackup,
        Staging,
        Journal,
        Replacement,
        InstalledValidation,
        EpochCommit,
        Rollback,
        Recovery
    };

    struct Result {
        bool success = false;
        bool restartRequired = false;
        bool rollbackSucceeded = false;
        Failure failure = Failure::None;
        QString error;
        QString safetyBackupPath;
        QString newEpoch;
    };

    struct RecoveryResult {
        bool success = true;
        bool recoveredPreviousDatabase = false;
        bool completedCommittedRestore = false;
        QString error;
    };

    static QString journalPath(const QString& stateDirectory);
    static bool isControlledRestoreArtifact(const QString& path, const QString& livePath,
                                            const QString& stateDirectory);
    static Result execute(const QString& candidatePath, const QString& livePath,
                          const QString& safetyBackupPath,
                          const QString& epochDirectory,
                          const QString& stateDirectory,
                          const FaultInjection* faults = nullptr,
                          Progress progress = {},
                          bool trustedPreflightArtifact = false);
    static RecoveryResult recoverAtStartup(const QString& livePath,
                                           const QString& epochDirectory,
                                           const QString& stateDirectory);
};

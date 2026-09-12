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

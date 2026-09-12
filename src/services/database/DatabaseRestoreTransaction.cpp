#include "DatabaseRestoreTransaction.h"

#include "../../database/DatabaseManager.h"
#include "../../network/HostDataEpoch.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {
constexpr auto JournalName = "database-restore-transaction.json";
constexpr auto StageSuffix = ".restore_candidate";
constexpr auto PreviousSuffix = ".restore_previous";
constexpr auto PreflightMarker = ".restore_preflight_";
const QStringList Sidecars{QStringLiteral("-wal"), QStringLiteral("-shm"),
                           QStringLiteral("-journal")};

struct Journal {
    QString state;
    QString live;
    QString stage;
    QString previous;
    QString oldEpoch;
    QString newEpoch;
};

bool sameFile(const QString& left, const QString& right)
{
    return QFileInfo(left).canonicalFilePath().compare(QFileInfo(right).canonicalFilePath(),
                                                        Qt::CaseInsensitive) == 0
        && !QFileInfo(left).canonicalFilePath().isEmpty();
}

bool isPreflightArtifact(const QString& path, const QString& livePath)
{
    return QFileInfo(path).absoluteFilePath().startsWith(
        QFileInfo(livePath).absoluteFilePath() + QString::fromLatin1(PreflightMarker),
        Qt::CaseInsensitive);
}

bool writeJournal(const QString& path, const Journal& value, QString* error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) *error = QStringLiteral("Unable to create the Restore state directory.");
        return false;
    }
    QSaveFile file(path);
    const QByteArray data = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 1}, {QStringLiteral("state"), value.state},
        {QStringLiteral("live"), value.live}, {QStringLiteral("stage"), value.stage},
        {QStringLiteral("previous"), value.previous},
        {QStringLiteral("oldEpoch"), value.oldEpoch},
        {QStringLiteral("newEpoch"), value.newEpoch}}).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error) *error = QStringLiteral("Unable to atomically persist Restore recovery state.");
        return false;
    }
    return true;
}

bool readJournal(const QString& path, Journal* value, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Restore recovery state could not be read.");
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse);
    const QJsonObject object = document.object();
    if (parse.error != QJsonParseError::NoError || !document.isObject()
        || object.value(QStringLiteral("version")).toInt(-1) != 1) {
        if (error) *error = QStringLiteral("Restore recovery state is malformed.");
        return false;
    }
    value->state = object.value(QStringLiteral("state")).toString();
    value->live = object.value(QStringLiteral("live")).toString();
    value->stage = object.value(QStringLiteral("stage")).toString();
    value->previous = object.value(QStringLiteral("previous")).toString();
    value->oldEpoch = object.value(QStringLiteral("oldEpoch")).toString();
    value->newEpoch = object.value(QStringLiteral("newEpoch")).toString();
    if ((value->state != QStringLiteral("prepared")
         && value->state != QStringLiteral("committed"))
        || value->live.isEmpty() || value->stage.isEmpty() || value->previous.isEmpty()
        || !HostDataEpoch::isValid(value->oldEpoch)
        || !HostDataEpoch::isValid(value->newEpoch)) {
        if (error) *error = QStringLiteral("Restore recovery state contains invalid fields.");
        return false;
    }
    return true;
}

bool removePath(const QString& path)
{
    return !QFile::exists(path) || QFile::remove(path);
}

bool moveWithSidecars(const QString& from, const QString& to)
{
    if (QFile::exists(to) && !QFile::remove(to)) return false;
    if (!QFile::rename(from, to)) return false;
    for (const QString& suffix : Sidecars) {
        const QString source = from + suffix;
        const QString destination = to + suffix;
        if (!QFile::exists(source)) continue;
        removePath(destination);
        if (!QFile::rename(source, destination)) return false;
    }
    return true;
}

void removeWithSidecars(const QString& path)
{
    removePath(path);
    for (const QString& suffix : Sidecars) removePath(path + suffix);
}

bool rollback(const Journal& journal, QString* error)
{
    removeWithSidecars(journal.live);
    if (!QFile::exists(journal.previous)
        || !moveWithSidecars(journal.previous, journal.live)) {
        if (error) *error = QStringLiteral("The previous database could not be restored from the filesystem rollback image.");
        return false;
    }
    const auto validation = DatabaseFileValidator::validate(journal.live);
    if (!validation.valid) {
        if (error) *error = QStringLiteral("The previous database was restored but failed validation: %1")
                                .arg(validation.message);
        return false;
    }
    return true;
}
}

QString DatabaseRestoreTransaction::journalPath(const QString& stateDirectory)
{
    return QDir(stateDirectory).filePath(QString::fromLatin1(JournalName));
}

bool DatabaseRestoreTransaction::isControlledRestoreArtifact(
    const QString& path, const QString& livePath, const QString& stateDirectory)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    return sameFile(absolute, livePath)
        || absolute.compare(livePath + QString::fromLatin1(StageSuffix), Qt::CaseInsensitive) == 0
        || absolute.compare(livePath + QString::fromLatin1(PreviousSuffix), Qt::CaseInsensitive) == 0
        || isPreflightArtifact(absolute, livePath)
        || absolute.compare(journalPath(stateDirectory), Qt::CaseInsensitive) == 0;
}

DatabaseRestoreTransaction::Result DatabaseRestoreTransaction::execute(
    const QString& candidatePath, const QString& livePath, const QString& safetyBackupPath,
    const QString& epochDirectory, const QString& stateDirectory,
    const FaultInjection* faults, Progress progress, bool trustedPreflightArtifact)
{
    Result result;
    result.safetyBackupPath = safetyBackupPath;
    if (progress) progress(QStringLiteral("Validating the selected Restore database..."));
    const auto candidate = DatabaseFileValidator::validate(candidatePath);
    const bool controlled = isControlledRestoreArtifact(candidatePath, livePath, stateDirectory);
    const bool permittedPreflight = trustedPreflightArtifact
        && isPreflightArtifact(candidatePath, livePath);
    if (!candidate.valid || (controlled && !permittedPreflight)) {
        result.failure = Failure::Candidate;
        result.error = candidate.valid
            ? QStringLiteral("The current database or a BrickSuite Restore artifact cannot be selected.")
            : candidate.message;
        return result;
    }
    const HostDataEpoch::LoadResult currentEpoch = HostDataEpoch::loadExisting(epochDirectory);
    if (!currentEpoch.success) {
        result.failure = Failure::EpochCommit;
        result.error = currentEpoch.error;
        return result;
    }

    if (progress) progress(QStringLiteral("Creating and verifying the pre-Restore safety backup..."));
    const auto safety = faults && faults->failSafetyBackup
        ? DatabaseManager::VerifiedBackupResult{
              false, DatabaseManager::BackupFailure::Snapshot,
              QStringLiteral("Injected safety-backup failure."), {}}
        : DatabaseManager::createVerifiedBackup(livePath, safetyBackupPath);
    if (!safety.success) {
        result.failure = Failure::SafetyBackup;
        result.error = safety.errorMessage;
        return result;
    }

    Journal journal;
    journal.state = QStringLiteral("prepared");
    journal.live = QFileInfo(livePath).absoluteFilePath();
    journal.stage = journal.live + QString::fromLatin1(StageSuffix);
    journal.previous = journal.live + QString::fromLatin1(PreviousSuffix);
    journal.oldEpoch = currentEpoch.epoch;
    journal.newEpoch = HostDataEpoch::create();
    result.newEpoch = journal.newEpoch;

    if (progress) progress(QStringLiteral("Staging the selected Restore database..."));
    removeWithSidecars(journal.stage);
    removeWithSidecars(journal.previous);
    if (!QFile::copy(candidatePath, journal.stage)) {
        result.failure = Failure::Staging;
        result.error = QStringLiteral("The selected backup could not be staged beside the live database.");
        return result;
    }
    QString error;
    const QString transactionPath = journalPath(stateDirectory);
    if (!writeJournal(transactionPath, journal, &error)) {
        removeWithSidecars(journal.stage);
        result.failure = Failure::Journal;
        result.error = error;
        return result;
    }

    if (progress) progress(QStringLiteral("Replacing the authoritative database..."));
    const bool movedLive = moveWithSidecars(journal.live, journal.previous);
    if (!movedLive || (faults && faults->failReplacement)
        || !QFile::rename(journal.stage, journal.live)) {
        result.failure = Failure::Replacement;
        result.error = QStringLiteral("The staged database could not be installed.");
    } else {
        if (progress) progress(QStringLiteral("Verifying the restored database..."));
        auto installed = DatabaseFileValidator::validate(journal.live);
        if (faults && faults->failInstalledValidation) {
            installed.valid = false;
            installed.message = QStringLiteral("Injected installed-database validation failure.");
        }
        if (!installed.valid) {
            result.failure = Failure::InstalledValidation;
            result.error = QStringLiteral("The installed database failed validation: %1")
                               .arg(installed.message);
        } else if ((faults && faults->failEpochCommit)
                   || !HostDataEpoch::persist(journal.newEpoch, &error, epochDirectory)) {
            result.failure = Failure::EpochCommit;
            result.error = faults && faults->failEpochCommit
                ? QStringLiteral("Injected data-epoch persistence failure.") : error;
        } else {
            if (progress) progress(QStringLiteral("Finalizing the restored database generation..."));
            journal.state = QStringLiteral("committed");
            if (!writeJournal(transactionPath, journal, &error)) {
                // Epoch B is already durable. Startup recognizes Epoch B with a
                // prepared journal as a committed Restore and never rolls back.
                result.error = error;
            }
            result.success = true;
            result.restartRequired = true;
            removeWithSidecars(journal.previous);
            removeWithSidecars(journal.stage);
            QFile::remove(transactionPath);
            return result;
        }
    }

    if (progress) progress(QStringLiteral("Recovering the previous authoritative database..."));
    QString rollbackError;
    result.rollbackSucceeded = !(faults && faults->failRollback)
        && rollback(journal, &rollbackError);
    if (faults && faults->failRollback)
        rollbackError = QStringLiteral("Injected filesystem rollback failure.");
    if (result.rollbackSucceeded) {
        HostDataEpoch::persist(journal.oldEpoch, nullptr, epochDirectory);
        removeWithSidecars(journal.stage);
        QFile::remove(transactionPath);
    } else {
        result.failure = Failure::Rollback;
        result.error += QStringLiteral("\n\nCritical recovery failure: ") + rollbackError;
    }
    return result;
}

DatabaseRestoreTransaction::RecoveryResult DatabaseRestoreTransaction::recoverAtStartup(
    const QString& livePath, const QString& epochDirectory, const QString& stateDirectory)
{
    RecoveryResult result;
    const QString path = journalPath(stateDirectory);
    if (!QFile::exists(path)) return result;
    Journal journal;
    if (!readJournal(path, &journal, &result.error)
        || QFileInfo(journal.live).absoluteFilePath()
               .compare(QFileInfo(livePath).absoluteFilePath(), Qt::CaseInsensitive) != 0) {
        result.success = false;
        if (result.error.isEmpty())
            result.error = QStringLiteral("Restore recovery state refers to a different database path.");
        return result;
    }
    const HostDataEpoch::LoadResult epoch = HostDataEpoch::loadExisting(epochDirectory);
    if (!epoch.success) {
        result.success = false;
        result.error = epoch.error;
        return result;
    }

    if (epoch.epoch == journal.newEpoch) {
        const auto validation = DatabaseFileValidator::validate(journal.live);
        if (!validation.valid) {
            result.success = false;
            result.error = QStringLiteral("The committed restored database failed startup validation: %1")
                               .arg(validation.message);
            return result;
        }
        removeWithSidecars(journal.previous);
        removeWithSidecars(journal.stage);
        QFile::remove(path);
        result.completedCommittedRestore = true;
        return result;
    }
    if (epoch.epoch != journal.oldEpoch) {
        result.success = false;
        result.error = QStringLiteral("Restore recovery state does not match the durable Host data epoch.");
        return result;
    }
    QString rollbackError;
    if (!rollback(journal, &rollbackError)) {
        result.success = false;
        result.error = rollbackError;
        return result;
    }
    removeWithSidecars(journal.stage);
    QFile::remove(path);
    result.recoveredPreviousDatabase = true;
    return result;
}

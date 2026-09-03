#include "AutomaticBackupPolicy.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTemporaryFile>
#include <QTimeZone>
#include <QtGlobal>

namespace {
bool writableCandidate(const QString& path)
{
    const QFileInfo target(path);
    if (target.exists() && !target.isDir())
        return false;
    QDir existing = target.exists() ? QDir(target.absoluteFilePath()) : target.dir();
    while (!existing.exists() && existing.cdUp()) {}
    const QFileInfo existingInfo(existing.absolutePath());
    if (!existing.exists() || !existingInfo.isDir() || !existingInfo.isWritable())
        return false;
    QTemporaryFile probe(existing.filePath("BrickSuiteBackupProbe_XXXXXX"));
    return probe.open();
}
}

QList<int> AutomaticBackupPolicy::supportedFrequencyHours()
{ return {1, 4, 8, 16, 24, 48, 168}; }

QString AutomaticBackupPolicy::frequencyDisplayText(int hours)
{
    hours = normalizeFrequencyHours(hours);
    if (hours == 24) return "Every 1 day";
    if (hours == 48) return "Every 2 days";
    if (hours == 168) return "Every 7 days";
    return QString("Every %1 hour%2").arg(hours).arg(hours == 1 ? QString() : "s");
}
int AutomaticBackupPolicy::normalizeRetentionCount(int value)
{ return qBound(MinimumRetentionCount, value, MaximumRetentionCount); }

QString AutomaticBackupPolicy::versionDirectory(const QString& root, int schemaVersion)
{ return QDir(root.trimmed()).filePath(QString("v%1").arg(schemaVersion)); }

QString AutomaticBackupPolicy::initialRootSuggestion()
{
#ifdef Q_OS_WIN
    const QStorageInfo preferred(QStringLiteral("D:/"));
    const QString dRoot = QStringLiteral("D:/Backup/BrickSuite/database");
    if (preferred.isValid() && preferred.isReady() && !preferred.isReadOnly()
        && writableCandidate(dRoot))
        return QDir::cleanPath(dRoot);
#endif
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString documentsRoot = QDir(documents).filePath("BrickSuite Backups/database");
    if (!documents.isEmpty() && writableCandidate(documentsRoot))
        return QDir::cleanPath(documentsRoot);
    return QDir::cleanPath(QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation)).filePath("backups/database"));
}

bool AutomaticBackupPolicy::validateRoot(const QString& root, QString* errorMessage)
{
    const QString trimmed = root.trimmed();
    if (trimmed.isEmpty()) {
        if (errorMessage) *errorMessage = "Select a backup root.";
        return false;
    }
    if (!writableCandidate(trimmed)) {
        if (errorMessage)
            *errorMessage = QString("The backup root is unavailable or not writable:\n%1")
                                .arg(QDir::toNativeSeparators(trimmed));
        return false;
    }
    return true;
}

QString AutomaticBackupPolicy::backupFileName(int schemaVersion, const QDateTime& timestampUtc)
{
    return QString("BrickSuite_AutoBackup_v%1_%2.db").arg(schemaVersion).arg(
        timestampUtc.toUTC().toString("yyyy-MM-dd_HHmmss"));
}

QString AutomaticBackupPolicy::unusedBackupPath(const QString& directory, int schemaVersion,
                                                const QDateTime& timestampUtc)
{
    QDateTime candidate = timestampUtc.toUTC();
    QString path;
    do {
        path = QDir(directory).filePath(backupFileName(schemaVersion, candidate));
        candidate = candidate.addSecs(1);
    } while (QFileInfo::exists(path));
    return path;
}

bool AutomaticBackupPolicy::parseBackupFileName(const QString& fileName,
                                                AutomaticBackupFileInfo* info)
{
    static const QRegularExpression pattern(
        QStringLiteral("^BrickSuite_AutoBackup_v([0-9]+)_([0-9]{4}-[0-9]{2}-[0-9]{2})_([0-9]{6})\\.db$"));
    const auto match = pattern.match(fileName);
    if (!match.hasMatch()) return false;
    bool ok = false;
    const int version = match.captured(1).toInt(&ok);
    const QDateTime parsedTimestamp = QDateTime::fromString(
        match.captured(2) + "_" + match.captured(3), "yyyy-MM-dd_HHmmss");
    if (!ok || version < 1 || !parsedTimestamp.isValid()) return false;
    if (info) {
        info->schemaVersion = version;
        info->timestampUtc = QDateTime(parsedTimestamp.date(), parsedTimestamp.time(),
                                       QTimeZone::UTC);
    }
    return true;
}

bool AutomaticBackupPolicy::isRetentionCandidate(const QString& fileName, int currentSchemaVersion)
{
    AutomaticBackupFileInfo info;
    return parseBackupFileName(fileName, &info) && info.schemaVersion == currentSchemaVersion;
}

bool AutomaticBackupPolicy::isDue(bool enabled, const QString& root, int frequencyHours,
                                  const QDateTime& lastSuccessfulUtc, const QDateTime& nowUtc)
{
    if (!enabled || root.trimmed().isEmpty() || !nowUtc.isValid()) return false;
    return !lastSuccessfulUtc.isValid()
        || nowUtc.toUTC() >= lastSuccessfulUtc.toUTC().addSecs(
               normalizeFrequencyHours(frequencyHours) * 60 * 60);
}

bool AutomaticBackupPolicy::retryEligible(int frequencyHours,
                                          const QDateTime& lastAttemptUtc,
                                          const QDateTime& nowUtc)
{
    const int retryHours = qBound(1, normalizeFrequencyHours(frequencyHours), MaximumRetryHours);
    return !lastAttemptUtc.isValid()
        || nowUtc.toUTC() >= lastAttemptUtc.toUTC().addSecs(retryHours * 60 * 60);
}

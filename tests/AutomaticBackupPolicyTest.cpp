#include "../src/services/database/AutomaticBackupPolicy.h"
#include "../src/services/database/AutomaticBackupRetention.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimeZone>

namespace {
bool require(bool condition, const QString& message)
{
    if (!condition) QTextStream(stderr) << "FAILED: " << message << '\n';
    return condition;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    const QList<int> supportedHours = {1, 4, 8, 16, 24, 48, 168};
    ok &= require(AutomaticBackupPolicy::supportedFrequencyHours() == supportedHours,
                  "supported frequency choices");
    ok &= require(AutomaticBackupPolicy::normalizeFrequencyHours(0) == 24,
                  "invalid frequency uses 24-hour default");
    ok &= require(AutomaticBackupPolicy::frequencyDisplayText(1) == "Every 1 hour"
                      && AutomaticBackupPolicy::frequencyDisplayText(4) == "Every 4 hours"
                      && AutomaticBackupPolicy::frequencyDisplayText(24) == "Every 1 day"
                      && AutomaticBackupPolicy::frequencyDisplayText(48) == "Every 2 days"
                      && AutomaticBackupPolicy::frequencyDisplayText(168) == "Every 7 days",
                  "natural frequency display text");
    ok &= require(AutomaticBackupPolicy::normalizeRetentionCount(0) == 1, "retention minimum");
    ok &= require(AutomaticBackupPolicy::versionDirectory("C:/Backup/database", 29)
                      == QDir("C:/Backup/database").filePath("v29"), "version directory");

    const QDateTime timestamp(QDate(2026, 9, 3), QTime(14, 25, 30), QTimeZone::UTC);
    const QString name = AutomaticBackupPolicy::backupFileName(29, timestamp);
    ok &= require(name == "BrickSuite_AutoBackup_v29_2026-09-03_142530.db", "filename");
    AutomaticBackupFileInfo parsed;
    ok &= require(AutomaticBackupPolicy::parseBackupFileName(name, &parsed), "parse valid");
    ok &= require(parsed.schemaVersion == 29 && parsed.timestampUtc == timestamp, "parsed values");
    ok &= require(!AutomaticBackupPolicy::parseBackupFileName(name + ".tmp"), "reject suffix");
    ok &= require(!AutomaticBackupPolicy::parseBackupFileName("BrickSuite_Backup_2026.db"), "reject manual");
    ok &= require(!AutomaticBackupPolicy::parseBackupFileName(
                      "BrickSuite_AutoBackup_v29_2026-02-30_142530.db"), "reject invalid date");
    ok &= require(AutomaticBackupPolicy::isRetentionCandidate(name, 29), "current candidate");
    ok &= require(!AutomaticBackupPolicy::isRetentionCandidate(name, 30), "other version");

    QTemporaryDir directory;
    const QString first = QDir(directory.path()).filePath(name);
    QFile marker(first);
    ok &= require(marker.open(QIODevice::WriteOnly), "create collision marker");
    marker.close();
    ok &= require(AutomaticBackupPolicy::unusedBackupPath(directory.path(), 29, timestamp)
                      .endsWith("2026-09-03_142531.db"), "collision advances timestamp");

    const QDateTime now = timestamp.addSecs(200 * 60 * 60);
    ok &= require(!AutomaticBackupPolicy::isDue(false, "root", 24, {}, now), "disabled not due");
    ok &= require(AutomaticBackupPolicy::isDue(true, "root", 24, {}, now), "never backed up due");
    for (const int hours : supportedHours) {
        ok &= require(!AutomaticBackupPolicy::isDue(
                          true, "root", hours, timestamp,
                          timestamp.addSecs(hours * 60 * 60 - 1)),
                      QString("%1-hour interval not due early").arg(hours));
        ok &= require(AutomaticBackupPolicy::isDue(
                          true, "root", hours, timestamp,
                          timestamp.addSecs(hours * 60 * 60)),
                      QString("%1-hour interval due on boundary").arg(hours));
        const int retryHours = qMin(hours, 6);
        ok &= require(!AutomaticBackupPolicy::retryEligible(
                          hours, timestamp,
                          timestamp.addSecs(retryHours * 60 * 60 - 1)),
                      QString("%1-hour retry gated early").arg(hours));
        ok &= require(AutomaticBackupPolicy::retryEligible(
                          hours, timestamp,
                          timestamp.addSecs(retryHours * 60 * 60)),
                      QString("%1-hour retry eligible on boundary").arg(hours));
    }
    ok &= require(!AutomaticBackupPolicy::initialRootSuggestion().isEmpty(), "initial root suggestion");

    const QString currentDir = QDir(directory.path()).filePath("v29");
    const QString oldDir = QDir(directory.path()).filePath("v28");
    QDir().mkpath(currentDir);
    QDir().mkpath(oldDir);
    auto createMarker = [&](const QString& path) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly);
    };
    const QString oldest = QDir(currentDir).filePath(
        "BrickSuite_AutoBackup_v29_2026-09-01_100000.db");
    const QString middle = QDir(currentDir).filePath(
        "BrickSuite_AutoBackup_v29_2026-09-02_100000.db");
    const QString newest = QDir(currentDir).filePath(
        "BrickSuite_AutoBackup_v29_2026-09-03_100000.db");
    const QString manual = QDir(currentDir).filePath("BrickSuite_Backup_manual.db");
    const QString malformed = QDir(currentDir).filePath(
        "BrickSuite_AutoBackup_v29_bad.db");
    const QString pending = newest + ".pending";
    const QString oldVersion = QDir(oldDir).filePath(
        "BrickSuite_AutoBackup_v28_2026-01-01_100000.db");
    ok &= require(createMarker(oldest) && createMarker(middle) && createMarker(newest)
                      && createMarker(manual) && createMarker(malformed)
                      && createMarker(pending) && createMarker(oldVersion),
                  "create retention fixtures");
    const auto cleanup = AutomaticBackupRetention::clean(currentDir, 29, 2, newest);
    ok &= require(cleanup.success && cleanup.removedFiles == QStringList{oldest},
                  "retention deletes deterministic oldest only");
    ok &= require(QFileInfo::exists(middle) && QFileInfo::exists(newest), "new backup protected");
    ok &= require(QFileInfo::exists(manual) && QFileInfo::exists(malformed)
                      && QFileInfo::exists(pending) && QFileInfo::exists(oldVersion),
                  "manual malformed pending and old-version files untouched");

    ok &= require(createMarker(oldest), "recreate oldest fixture");
    QStringList attempted;
    const auto failedCleanup = AutomaticBackupRetention::clean(
        currentDir, 29, 1, newest, [&](const QString& path) {
            attempted.append(path);
            return false;
        });
    ok &= require(!failedCleanup.success && attempted == QStringList{oldest},
                  "deletion failure stops cleanup immediately");
    ok &= require(QFileInfo::exists(oldest) && QFileInfo::exists(middle)
                      && QFileInfo::exists(newest), "deletion failure preserves backups");

    if (ok) QTextStream(stdout) << "AutomaticBackupPolicyTest passed\n";
    return ok ? 0 : 1;
}

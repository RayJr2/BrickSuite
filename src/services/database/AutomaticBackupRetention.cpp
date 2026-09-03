#include "AutomaticBackupRetention.h"

#include "AutomaticBackupPolicy.h"

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QVector>

AutomaticBackupRetentionResult AutomaticBackupRetention::clean(
    const QString& versionDirectory, int currentSchemaVersion, int retentionCount,
    const QString& protectedBackupPath, const RemoveFile& removeFile)
{
    AutomaticBackupRetentionResult result;
    struct Candidate { QString path; QString name; QDateTime timestamp; };
    QVector<Candidate> candidates;
    const QDir directory(versionDirectory);
    const QString protectedPath = QFileInfo(protectedBackupPath).absoluteFilePath();

    for (const QFileInfo& file : directory.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        AutomaticBackupFileInfo parsed;
        if (!AutomaticBackupPolicy::parseBackupFileName(file.fileName(), &parsed)
            || parsed.schemaVersion != currentSchemaVersion)
            continue;
        candidates.append({file.absoluteFilePath(), file.fileName(), parsed.timestampUtc});
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
        if (left.timestamp != right.timestamp) return left.timestamp < right.timestamp;
        return left.name < right.name;
    });

    int excess = candidates.size()
                 - AutomaticBackupPolicy::normalizeRetentionCount(retentionCount);
    const RemoveFile remover = removeFile ? removeFile : [](const QString& path) {
        return QFile::remove(path);
    };
    for (const Candidate& candidate : candidates) {
        if (excess <= 0) break;
        if (candidate.path == protectedPath) continue;
        if (!remover(candidate.path)) {
            result.success = false;
            result.errorMessage = QString("Database backup completed and verified, but an older "
                                          "automatic backup could not be removed: %1")
                                      .arg(candidate.path);
            break;
        }
        result.removedFiles.append(candidate.path);
        --excess;
    }
    return result;
}

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

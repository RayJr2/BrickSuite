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

#include <QDateTime>
#include <QList>
#include <QString>

struct AutomaticBackupFileInfo
{
    int schemaVersion = 0;
    QDateTime timestampUtc;
};

class AutomaticBackupPolicy
{
public:
    static constexpr int DefaultFrequencyHours = 24;
    static constexpr int DefaultRetentionCount = 14;
    static constexpr int MinimumRetentionCount = 1;
    static constexpr int MaximumRetentionCount = 365;
    static constexpr int MaximumRetryHours = 6;

    static QList<int> supportedFrequencyHours();
    static constexpr bool isSupportedFrequencyHours(int value)
    {
        return value == 1 || value == 4 || value == 8 || value == 16
               || value == 24 || value == 48 || value == 168;
    }
    static constexpr int normalizeFrequencyHours(int value)
    { return isSupportedFrequencyHours(value) ? value : DefaultFrequencyHours; }
    static QString frequencyDisplayText(int hours);
    static int normalizeRetentionCount(int value);
    static QString versionDirectory(const QString& root, int schemaVersion);
    static QString initialRootSuggestion();
    static bool validateRoot(const QString& root, QString* errorMessage = nullptr);
    static QString backupFileName(int schemaVersion, const QDateTime& timestampUtc);
    static QString unusedBackupPath(const QString& directory, int schemaVersion,
                                    const QDateTime& timestampUtc);
    static bool parseBackupFileName(const QString& fileName, AutomaticBackupFileInfo* info = nullptr);
    static bool isRetentionCandidate(const QString& fileName, int currentSchemaVersion);
    static bool isDue(bool enabled, const QString& root, int frequencyHours,
                      const QDateTime& lastSuccessfulUtc, const QDateTime& nowUtc);
    static bool retryEligible(int frequencyHours, const QDateTime& lastAttemptUtc,
                              const QDateTime& nowUtc);
};

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

#include <QByteArray>
#include <QString>

class UserSettings
{
public:
    enum class Theme { System, Light, Dark };

    static UserSettings& instance();

    Theme theme() const;
    void setTheme(Theme theme);

    int resultsPerPage() const;
    void setResultsPerPage(int resultsPerPage);

    int defaultWorkspaceId() const;
    void setDefaultWorkspaceId(int workspaceId);

    QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray& geometry);

    QByteArray mainWindowState() const;
    void setMainWindowState(const QByteArray& state);

    QByteArray logViewerGeometry() const;
    void setLogViewerGeometry(const QByteArray& geometry);

    bool showArchivedBuilds() const;
    void setShowArchivedBuilds(bool showArchived);

    static QString themeToString(Theme theme);
    static Theme themeFromString(const QString& value);

    QString rebrickableApiKey() const;
    void setRebrickableApiKey(const QString& apiKey);

    static constexpr int DefaultRebrickableRequestIntervalMs = 1250;
    static constexpr int MinimumRebrickableRequestIntervalMs = 1000;
    static constexpr int MaximumRebrickableRequestIntervalMs = 60000;

    int rebrickableMinimumRequestIntervalMs() const;

    void setRebrickableMinimumRequestIntervalMs(int intervalMs);

private:
    UserSettings() = default;

    UserSettings(const UserSettings&) = delete;
    UserSettings& operator=(const UserSettings&) = delete;

    static constexpr int DefaultResultsPerPage = 250;
};
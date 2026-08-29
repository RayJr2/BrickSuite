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

    QByteArray helpViewerGeometry() const;
    void setHelpViewerGeometry(const QByteArray& geometry);

    QByteArray addInventoryDialogGeometry() const;
    void setAddInventoryDialogGeometry(const QByteArray& geometry);

    bool addInventoryTryBrickLinkId() const;
    void setAddInventoryTryBrickLinkId(bool enabled);

    QByteArray partReferenceGeometry() const;
    void setPartReferenceGeometry(const QByteArray& geometry);

    QString partReferenceCatalog() const;
    void setPartReferenceCatalog(const QString& catalog);

    QByteArray partReferenceSplitterState() const;
    void setPartReferenceSplitterState(const QByteArray& state);

    QString partReferenceViewMode(const QString& catalog) const;
    void setPartReferenceViewMode(const QString& catalog, const QString& viewMode);

    // Legacy prototype settings retained for backward compatibility.
    int partReferenceGroupIndex() const;
    void setPartReferenceGroupIndex(int index);

    int partReferencePageIndex(int groupIndex) const;
    void setPartReferencePageIndex(int groupIndex, int pageIndex);

    QByteArray helpViewerSplitterState() const;
    void setHelpViewerSplitterState(const QByteArray& state);

    bool showArchivedBuilds() const;
    void setShowArchivedBuilds(bool showArchived);

    static QString themeToString(Theme theme);
    static Theme themeFromString(const QString& value);

    QString rebrickableApiKey() const;
    bool setRebrickableApiKey(const QString& apiKey, QString* error = nullptr);

    bool rebrickableConnectionPreviouslyVerified() const;
    void setRebrickableConnectionPreviouslyVerified(bool verified);

    QString bricksetApiKey() const;
    bool setBricksetApiKey(const QString& apiKey, QString* error = nullptr);

    bool bricksetConnectionPreviouslyVerified() const;
    void setBricksetConnectionPreviouslyVerified(bool verified);

    static constexpr int DefaultBricksetDailyGetSetsThreshold = 80;
    static constexpr int MinimumBricksetDailyGetSetsThreshold = 1;
    static constexpr int MaximumBricksetDailyGetSetsThreshold = 10000;

    int bricksetDailyGetSetsThreshold() const;
    void setBricksetDailyGetSetsThreshold(int threshold);

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
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

#include <QDialog>

#include "../../api/ApiConnectionStatus.h"

class WorkspaceContext;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QLabel;
class QCheckBox;
class QPushButton;
class QTabWidget;
class QSpinBox;
class QWidget;

class RebrickableApiClient;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(WorkspaceContext& workspaceContext, QWidget* parent = nullptr);

signals:
    void settingsChanged();

private slots:
    void saveSettings();
    void cancelSettings();
    void previewTheme(int index);
    void showApiKeyToggled(bool checked);
    void testRebrickableConnection();

private:
    void buildGeneralTab();
    void buildAppearanceTab();
    void buildApisTab();
    QWidget* buildRebrickableApiPage(QWidget* parent);

    void setRebrickableConnectionStatus(ApiConnectionStatus status);
    static QString apiConnectionStatusText(ApiConnectionStatus status);
    void startRebrickableConnectionTest(const QString& apiKey);

    void loadSettings();
    void loadWorkspaces();

    WorkspaceContext& m_workspaceContext;

    QTabWidget* m_tabWidget = nullptr;

    // General
    QComboBox* m_resultsPerPageCombo = nullptr;
    QComboBox* m_defaultWorkspaceCombo = nullptr;

    // Appearance
    QComboBox* m_themeCombo = nullptr;
    int m_originalThemeValue = 0;

    // APIs / Rebrickable
    QTabWidget* m_apiTabWidget = nullptr;
    QLineEdit* m_apiKeyEdit = nullptr;
    QLabel* m_rebrickableStatusLabel = nullptr;
    QCheckBox* m_showApiKeyCheck = nullptr;
    QPushButton* m_testConnectionButton = nullptr;
    QSpinBox* m_rebrickableRequestIntervalSpin = nullptr;
    ApiConnectionStatus m_rebrickableConnectionStatus = ApiConnectionStatus::NotConfigured;
    QString m_originalRebrickableApiKey;

    RebrickableApiClient* m_rebrickableApiClient = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;
};
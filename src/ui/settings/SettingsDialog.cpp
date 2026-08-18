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

#include "SettingsDialog.h"

#include "../../app/WorkspaceContext.h"
#include "../help/HelpManager.h"
#include "../../models/Workspace.h"
#include "../../repositories/WorkspaceRepository.h"
#include "../../services/RebrickableApiClient.h"
#include "../../settings/ThemeManager.h"
#include "../../settings/UserSettings.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

SettingsDialog::SettingsDialog(WorkspaceContext& workspaceContext, QWidget* parent)
    : QDialog(parent)
    , m_workspaceContext(workspaceContext)
{
    setWindowTitle("BrickSuite Settings");

    resize(600, 500);

    //
    // Context-sensitive Help for the entire Settings dialog. Using
    // WidgetWithChildrenShortcut means F1 works while focus is inside
    // any Settings control (combo box, edit, checkbox, button, etc.).
    //
    auto* helpAction = new QAction(this);
    helpAction->setShortcut(QKeySequence::HelpContents);
    helpAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    connect(helpAction, &QAction::triggered, this, [this]() {
        HelpManager::showTopic(HelpTopic::Settings, this);
    });

    addAction(helpAction);

    auto* mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);

    mainLayout->addWidget(m_tabWidget);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    mainLayout->addWidget(m_buttonBox);

    buildGeneralTab();
    buildAppearanceTab();
    buildApisTab();

    m_rebrickableApiClient = new RebrickableApiClient(this);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::cancelSettings);

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::connectionTestFinished,
            this,
            [this](const RebrickableApiClient::ConnectionResult& result) {
                m_testConnectionButton->setEnabled(true);

                UserSettings& settings = UserSettings::instance();

                if (result.success) {
                    setRebrickableConnectionStatus(ApiConnectionStatus::Connected);
                    settings.setRebrickableConnectionPreviouslyVerified(true);
                } else {
                    settings.setRebrickableConnectionPreviouslyVerified(false);

                    if (result.httpStatusCode == 401) {
                        setRebrickableConnectionStatus(ApiConnectionStatus::AuthenticationFailed);
                    } else if (result.httpStatusCode == 0) {
                        setRebrickableConnectionStatus(ApiConnectionStatus::NetworkError);
                    } else {
                        setRebrickableConnectionStatus(ApiConnectionStatus::ProviderError);
                    }
                }
            });

    loadWorkspaces();
    loadSettings();
}
void SettingsDialog::loadWorkspaces()
{
    m_defaultWorkspaceCombo->clear();

    m_defaultWorkspaceCombo->addItem("(None)", 0);

    WorkspaceRepository repository;

    const QList<Workspace> workspaces = repository.getAll();

    for (const Workspace& workspace : workspaces) {
        m_defaultWorkspaceCombo->addItem(workspace.name(), workspace.id());
    }
}

void SettingsDialog::loadSettings()
{
    UserSettings& settings = UserSettings::instance();

    const int resultsIndex = m_resultsPerPageCombo->findData(settings.resultsPerPage());

    if (resultsIndex >= 0) {
        m_resultsPerPageCombo->setCurrentIndex(resultsIndex);
    }

    const int workspaceIndex = m_defaultWorkspaceCombo->findData(settings.defaultWorkspaceId());

    if (workspaceIndex >= 0) {
        m_defaultWorkspaceCombo->setCurrentIndex(workspaceIndex);
    } else {
        m_defaultWorkspaceCombo->setCurrentIndex(0);
    }

    m_originalThemeValue = static_cast<int>(settings.theme());

    const int themeIndex = m_themeCombo->findData(m_originalThemeValue);

    if (themeIndex >= 0) {
        m_themeCombo->setCurrentIndex(themeIndex);
    }

    m_originalRebrickableApiKey = settings.rebrickableApiKey();
    m_apiKeyEdit->setText(m_originalRebrickableApiKey);
    m_rebrickableRequestIntervalSpin->setValue(settings.rebrickableMinimumRequestIntervalMs());

    if (m_originalRebrickableApiKey.trimmed().isEmpty()) {
        setRebrickableConnectionStatus(ApiConnectionStatus::NotConfigured);
    } else if (settings.rebrickableConnectionPreviouslyVerified()) {
        setRebrickableConnectionStatus(ApiConnectionStatus::Testing);

        // Allow the Settings dialog to finish constructing before starting
        // the asynchronous provider validation.
        QTimer::singleShot(0, this, [this]() {
            startRebrickableConnectionTest(m_apiKeyEdit->text().trimmed());
        });
    } else {
        setRebrickableConnectionStatus(ApiConnectionStatus::Unknown);
    }
}

void SettingsDialog::saveSettings()
{
    UserSettings& settings = UserSettings::instance();

    const int resultsPerPage = m_resultsPerPageCombo->currentData().toInt();

    const int defaultWorkspaceId = m_defaultWorkspaceCombo->currentData().toInt();

    const auto theme = static_cast<UserSettings::Theme>(m_themeCombo->currentData().toInt());

    const QString apiKey = m_apiKeyEdit->text().trimmed();

    settings.setResultsPerPage(resultsPerPage);

    settings.setDefaultWorkspaceId(defaultWorkspaceId);

    settings.setTheme(theme);

    const bool rebrickableKeyChanged = (apiKey != m_originalRebrickableApiKey.trimmed());

    settings.setRebrickableApiKey(apiKey);

    if (rebrickableKeyChanged && m_rebrickableConnectionStatus != ApiConnectionStatus::Connected) {
        settings.setRebrickableConnectionPreviouslyVerified(false);
    }

    const int rebrickableRequestIntervalMs = m_rebrickableRequestIntervalSpin->value();
    settings.setRebrickableMinimumRequestIntervalMs(rebrickableRequestIntervalMs);

    if (QApplication* application = qobject_cast<QApplication*>(QApplication::instance())) {
        ThemeManager::applyTheme(*application, theme);
    }

    emit settingsChanged();

    accept();
}

void SettingsDialog::cancelSettings()
{
    const auto originalTheme = static_cast<UserSettings::Theme>(m_originalThemeValue);

    if (QApplication* application = qobject_cast<QApplication*>(QApplication::instance())) {
        ThemeManager::applyTheme(*application, originalTheme);
    }

    reject();
}

void SettingsDialog::previewTheme(int index)
{
    if (index < 0)
        return;

    const auto theme = static_cast<UserSettings::Theme>(m_themeCombo->itemData(index).toInt());

    if (QApplication* application = qobject_cast<QApplication*>(QApplication::instance())) {
        ThemeManager::applyTheme(*application, theme);
    }
}

void SettingsDialog::buildGeneralTab()
{
    auto* tab = new QWidget(m_tabWidget);

    auto* layout = new QVBoxLayout(tab);

    auto* generalGroup = new QGroupBox("General", tab);

    auto* generalLayout = new QFormLayout(generalGroup);

    m_resultsPerPageCombo = new QComboBox(generalGroup);

    m_resultsPerPageCombo->addItem("100", 100);

    m_resultsPerPageCombo->addItem("250", 250);

    m_resultsPerPageCombo->addItem("500", 500);

    m_defaultWorkspaceCombo = new QComboBox(generalGroup);

    generalLayout->addRow("Results per page:", m_resultsPerPageCombo);

    generalLayout->addRow("Default workspace:", m_defaultWorkspaceCombo);

    layout->addWidget(generalGroup);

    layout->addStretch();

    m_tabWidget->addTab(tab, "General");
}

void SettingsDialog::buildAppearanceTab()
{
    auto* tab = new QWidget(m_tabWidget);

    auto* layout = new QVBoxLayout(tab);

    auto* appearanceGroup = new QGroupBox("Appearance", tab);

    auto* appearanceLayout = new QFormLayout(appearanceGroup);

    m_themeCombo = new QComboBox(appearanceGroup);

    m_themeCombo->addItem("System", static_cast<int>(UserSettings::Theme::System));

    m_themeCombo->addItem("Light", static_cast<int>(UserSettings::Theme::Light));

    m_themeCombo->addItem("Dark", static_cast<int>(UserSettings::Theme::Dark));

    connect(m_themeCombo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &SettingsDialog::previewTheme);

    appearanceLayout->addRow("Theme:", m_themeCombo);

    layout->addWidget(appearanceGroup);

    layout->addStretch();

    m_tabWidget->addTab(tab, "Appearance");
}

void SettingsDialog::buildApisTab()
{
    auto* tab = new QWidget(m_tabWidget);
    auto* layout = new QVBoxLayout(tab);

    m_apiTabWidget = new QTabWidget(tab);
    m_apiTabWidget->setTabPosition(QTabWidget::North);

    m_apiTabWidget->addTab(buildRebrickableApiPage(m_apiTabWidget), "Rebrickable");

    // BrickLink is the next provider planned for M14.4. Its provider-specific
    // configuration controls will be added during M14.3 as the authentication
    // requirements are implemented. Brickset can be added later without
    // changing the top-level Settings layout.

    layout->addWidget(m_apiTabWidget);

    m_tabWidget->addTab(tab, "APIs");
}

QWidget* SettingsDialog::buildRebrickableApiPage(QWidget* parent)
{
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);

    auto* apiGroup = new QGroupBox("Rebrickable API", page);
    auto* apiLayout = new QFormLayout(apiGroup);

    m_apiKeyEdit = new QLineEdit(apiGroup);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText("Enter Rebrickable API key");

    m_showApiKeyCheck = new QCheckBox("Show API key", apiGroup);

    m_rebrickableStatusLabel = new QLabel(apiConnectionStatusText(ApiConnectionStatus::NotConfigured),
                                         apiGroup);

    connect(m_apiKeyEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_rebrickableConnectionStatus == ApiConnectionStatus::Testing)
            return;

        setRebrickableConnectionStatus(text.trimmed().isEmpty()
                                           ? ApiConnectionStatus::NotConfigured
                                           : ApiConnectionStatus::Unknown);
    });

    connect(m_apiKeyEdit, &QLineEdit::editingFinished, this, [this]() {
        const QString apiKey = m_apiKeyEdit->text().trimmed();

        if (!apiKey.isEmpty() && apiKey != m_originalRebrickableApiKey
            && m_rebrickableConnectionStatus != ApiConnectionStatus::Testing
            && m_rebrickableConnectionStatus != ApiConnectionStatus::Connected) {
            startRebrickableConnectionTest(apiKey);
        }
    });

    m_rebrickableRequestIntervalSpin = new QSpinBox(apiGroup);
    m_rebrickableRequestIntervalSpin->setRange(UserSettings::MinimumRebrickableRequestIntervalMs,
                                               UserSettings::MaximumRebrickableRequestIntervalMs);
    m_rebrickableRequestIntervalSpin->setSingleStep(250);
    m_rebrickableRequestIntervalSpin->setSuffix(" ms");
    m_rebrickableRequestIntervalSpin->setToolTip("Minimum time between Rebrickable API requests.");

    m_testConnectionButton = new QPushButton("Test Connection", apiGroup);

    connect(m_testConnectionButton,
            &QPushButton::clicked,
            this,
            &SettingsDialog::testRebrickableConnection);

    apiLayout->addRow("API Key:", m_apiKeyEdit);
    apiLayout->addRow(QString(), m_showApiKeyCheck);
    apiLayout->addRow("Connection Status:", m_rebrickableStatusLabel);
    apiLayout->addRow("Minimum Request Interval:", m_rebrickableRequestIntervalSpin);
    apiLayout->addRow(QString(), m_testConnectionButton);

    auto* noteLabel = new QLabel("BrickSuite throttles all Rebrickable API requests "
                                 "through a shared request queue.\n\n"
                                 "Bulk catalog operations should use Rebrickable "
                                 "download files rather than repeated API requests.\n\n"
                                 "HTTP 429 responses indicate throttling. BrickSuite "
                                 "will stop further Rebrickable API requests for the "
                                 "current session if a 429 response is received.",
                                 apiGroup);
    noteLabel->setWordWrap(true);
    apiLayout->addRow(QString(), noteLabel);

    layout->addWidget(apiGroup);
    layout->addStretch();

    connect(m_showApiKeyCheck, &QCheckBox::toggled, this, &SettingsDialog::showApiKeyToggled);

    return page;
}

void SettingsDialog::showApiKeyToggled(bool checked)
{
    m_apiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}

void SettingsDialog::testRebrickableConnection()
{
    const QString apiKey = m_apiKeyEdit->text().trimmed();

    if (apiKey.isEmpty()) {
        setRebrickableConnectionStatus(ApiConnectionStatus::NotConfigured);
        QMessageBox::warning(this, "Rebrickable", "Enter your Rebrickable API key first.");
        return;
    }

    startRebrickableConnectionTest(apiKey);
}

void SettingsDialog::startRebrickableConnectionTest(const QString& apiKey)
{
    if (apiKey.trimmed().isEmpty())
        return;

    setRebrickableConnectionStatus(ApiConnectionStatus::Testing);
    m_testConnectionButton->setEnabled(false);
    m_rebrickableApiClient->testConnection(apiKey.trimmed());
}

void SettingsDialog::setRebrickableConnectionStatus(ApiConnectionStatus status)
{
    m_rebrickableConnectionStatus = status;

    if (m_rebrickableStatusLabel)
        m_rebrickableStatusLabel->setText(apiConnectionStatusText(status));
}

QString SettingsDialog::apiConnectionStatusText(ApiConnectionStatus status)
{
    switch (status) {
    case ApiConnectionStatus::NotConfigured:
        return QStringLiteral("Not Configured");
    case ApiConnectionStatus::Unknown:
        return QStringLiteral("Not Tested");
    case ApiConnectionStatus::Testing:
        return QStringLiteral("Testing...");
    case ApiConnectionStatus::Connected:
        return QStringLiteral("Connected");
    case ApiConnectionStatus::AuthenticationFailed:
        return QStringLiteral("Authentication Failed");
    case ApiConnectionStatus::NetworkError:
        return QStringLiteral("Network Error");
    case ApiConnectionStatus::ProviderError:
        return QStringLiteral("Provider Error");
    }

    return QStringLiteral("Unknown");
}
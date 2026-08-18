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
#include "../../api/brickset/BricksetService.h"
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
    m_bricksetService = new BricksetService(this);

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

    connect(m_bricksetService,
            &BricksetService::connectionTestFinished,
            this,
            [this](const BricksetService::ConnectionResult& result) {
                m_testBricksetConnectionButton->setEnabled(true);

                UserSettings& settings = UserSettings::instance();

                if (result.success) {
                    setBricksetConnectionStatus(ApiConnectionStatus::Connected);
                    settings.setBricksetConnectionPreviouslyVerified(true);

                    const QString apiKey = m_bricksetApiKeyEdit->text().trimmed();
                    if (!apiKey.isEmpty())
                        m_bricksetService->getKeyUsageStats(apiKey);
                } else {
                    settings.setBricksetConnectionPreviouslyVerified(false);

                    switch (result.error.type) {
                    case ApiErrorType::Authentication:
                        setBricksetConnectionStatus(ApiConnectionStatus::AuthenticationFailed);
                        break;
                    case ApiErrorType::Network:
                    case ApiErrorType::Timeout:
                        setBricksetConnectionStatus(ApiConnectionStatus::NetworkError);
                        break;
                    default:
                        setBricksetConnectionStatus(ApiConnectionStatus::ProviderError);
                        break;
                    }
                }
            });

    connect(m_bricksetService,
            &BricksetService::keyUsageStatsFinished,
            this,
            [this](const BricksetService::KeyUsageResult& result) {
                if (!m_bricksetUsageLabel)
                    return;

                if (result.success) {
                    m_bricksetUsageLabel->setText(
                        QString("%1 calls today").arg(
                            BricksetService::effectiveTodayGetSetsCount()));
                } else {
                    m_bricksetUsageLabel->setText("Unavailable");
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

    m_originalBricksetApiKey = settings.bricksetApiKey();
    m_bricksetApiKeyEdit->setText(m_originalBricksetApiKey);
    m_bricksetDailyThresholdSpin->setValue(settings.bricksetDailyGetSetsThreshold());

    if (BricksetService::keyUsageKnown()) {
        m_bricksetUsageLabel->setText(
            QString("%1 calls today").arg(
                BricksetService::effectiveTodayGetSetsCount()));
    } else {
        m_bricksetUsageLabel->setText("Not checked");
    }

    if (m_originalBricksetApiKey.trimmed().isEmpty()) {
        setBricksetConnectionStatus(ApiConnectionStatus::NotConfigured);
    } else if (settings.bricksetConnectionPreviouslyVerified()) {
        setBricksetConnectionStatus(ApiConnectionStatus::Testing);

        QTimer::singleShot(0, this, [this]() {
            startBricksetConnectionTest(m_bricksetApiKeyEdit->text().trimmed());
        });
    } else {
        setBricksetConnectionStatus(ApiConnectionStatus::Unknown);
    }
}

void SettingsDialog::saveSettings()
{
    UserSettings& settings = UserSettings::instance();

    const int resultsPerPage = m_resultsPerPageCombo->currentData().toInt();

    const int defaultWorkspaceId = m_defaultWorkspaceCombo->currentData().toInt();

    const auto theme = static_cast<UserSettings::Theme>(m_themeCombo->currentData().toInt());

    const QString apiKey = m_apiKeyEdit->text().trimmed();
    const QString bricksetApiKey = m_bricksetApiKeyEdit->text().trimmed();

    settings.setResultsPerPage(resultsPerPage);

    settings.setDefaultWorkspaceId(defaultWorkspaceId);

    settings.setTheme(theme);

    const bool rebrickableKeyChanged = (apiKey != m_originalRebrickableApiKey.trimmed());

    settings.setRebrickableApiKey(apiKey);

    if (rebrickableKeyChanged && m_rebrickableConnectionStatus != ApiConnectionStatus::Connected) {
        settings.setRebrickableConnectionPreviouslyVerified(false);
    }

    const bool bricksetKeyChanged =
        (bricksetApiKey != m_originalBricksetApiKey.trimmed());

    settings.setBricksetApiKey(bricksetApiKey);

    if (bricksetKeyChanged && m_bricksetConnectionStatus != ApiConnectionStatus::Connected) {
        settings.setBricksetConnectionPreviouslyVerified(false);
    }

    settings.setBricksetDailyGetSetsThreshold(m_bricksetDailyThresholdSpin->value());

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
    m_apiTabWidget->addTab(buildBricksetApiPage(m_apiTabWidget), "Brickset");

    // BrickLink remains deferred because BrickLink currently restricts Store
    // API registration to seller accounts. It can be added later without
    // changing the Settings hierarchy.

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

QWidget* SettingsDialog::buildBricksetApiPage(QWidget* parent)
{
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);

    auto* apiGroup = new QGroupBox("Brickset API", page);
    auto* apiLayout = new QFormLayout(apiGroup);

    m_bricksetApiKeyEdit = new QLineEdit(apiGroup);
    m_bricksetApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_bricksetApiKeyEdit->setPlaceholderText("Enter Brickset API key");

    m_showBricksetApiKeyCheck = new QCheckBox("Show API key", apiGroup);

    m_bricksetStatusLabel =
        new QLabel(apiConnectionStatusText(ApiConnectionStatus::NotConfigured), apiGroup);

    m_bricksetUsageLabel = new QLabel("Not checked", apiGroup);

    m_bricksetDailyThresholdSpin = new QSpinBox(apiGroup);
    m_bricksetDailyThresholdSpin->setRange(
        UserSettings::MinimumBricksetDailyGetSetsThreshold,
        UserSettings::MaximumBricksetDailyGetSetsThreshold);
    m_bricksetDailyThresholdSpin->setValue(
        UserSettings::DefaultBricksetDailyGetSetsThreshold);
    m_bricksetDailyThresholdSpin->setSuffix(" calls/day");
    m_bricksetDailyThresholdSpin->setToolTip(
        "When today's effective Brickset getSets usage reaches this threshold, "
        "BrickSuite uses Rebrickable for Set Details instead.");

    connect(m_bricksetApiKeyEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_bricksetConnectionStatus == ApiConnectionStatus::Testing)
            return;

        setBricksetConnectionStatus(text.trimmed().isEmpty()
                                        ? ApiConnectionStatus::NotConfigured
                                        : ApiConnectionStatus::Unknown);

        if (text.trimmed() != m_originalBricksetApiKey.trimmed()) {
            BricksetService::invalidateKeyUsageCache();
            if (m_bricksetUsageLabel)
                m_bricksetUsageLabel->setText("Not checked");
        }
    });

    connect(m_bricksetApiKeyEdit, &QLineEdit::editingFinished, this, [this]() {
        const QString apiKey = m_bricksetApiKeyEdit->text().trimmed();

        if (!apiKey.isEmpty() && apiKey != m_originalBricksetApiKey
            && m_bricksetConnectionStatus != ApiConnectionStatus::Testing
            && m_bricksetConnectionStatus != ApiConnectionStatus::Connected) {
            startBricksetConnectionTest(apiKey);
        }
    });

    m_testBricksetConnectionButton = new QPushButton("Test Connection", apiGroup);

    connect(m_testBricksetConnectionButton,
            &QPushButton::clicked,
            this,
            &SettingsDialog::testBricksetConnection);

    apiLayout->addRow("API Key:", m_bricksetApiKeyEdit);
    apiLayout->addRow(QString(), m_showBricksetApiKeyCheck);
    apiLayout->addRow("Connection Status:", m_bricksetStatusLabel);
    apiLayout->addRow("Today's getSets Usage:", m_bricksetUsageLabel);
    apiLayout->addRow("Daily getSets Threshold:", m_bricksetDailyThresholdSpin);
    apiLayout->addRow(QString(), m_testBricksetConnectionButton);

    auto* noteLabel = new QLabel(
        "BrickSuite uses Brickset API v3. A previously verified API key is "
        "validated automatically when Settings opens.\n\n"
        "Brickset is preferred for Set Details enrichment while daily getSets "
        "usage remains below the configured threshold. Rebrickable is used "
        "automatically when the threshold is reached or Brickset is unavailable.",
        apiGroup);
    noteLabel->setWordWrap(true);
    apiLayout->addRow(QString(), noteLabel);

    layout->addWidget(apiGroup);
    layout->addStretch();

    connect(m_showBricksetApiKeyCheck,
            &QCheckBox::toggled,
            this,
            &SettingsDialog::showBricksetApiKeyToggled);

    return page;
}

void SettingsDialog::showApiKeyToggled(bool checked)
{
    m_apiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}

void SettingsDialog::showBricksetApiKeyToggled(bool checked)
{
    m_bricksetApiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
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

void SettingsDialog::testBricksetConnection()
{
    const QString apiKey = m_bricksetApiKeyEdit->text().trimmed();

    if (apiKey.isEmpty()) {
        setBricksetConnectionStatus(ApiConnectionStatus::NotConfigured);
        QMessageBox::warning(this, "Brickset", "Enter your Brickset API key first.");
        return;
    }

    startBricksetConnectionTest(apiKey);
}

void SettingsDialog::startBricksetConnectionTest(const QString& apiKey)
{
    if (apiKey.trimmed().isEmpty())
        return;

    setBricksetConnectionStatus(ApiConnectionStatus::Testing);
    m_testBricksetConnectionButton->setEnabled(false);
    m_bricksetService->testConnection(apiKey.trimmed());
}

void SettingsDialog::setBricksetConnectionStatus(ApiConnectionStatus status)
{
    m_bricksetConnectionStatus = status;

    if (m_bricksetStatusLabel)
        m_bricksetStatusLabel->setText(apiConnectionStatusText(status));
}


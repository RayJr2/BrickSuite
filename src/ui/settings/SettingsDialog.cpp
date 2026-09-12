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
#include "../../services/RebrickableApiClient.h"
#include "../../services/application/ApplicationServices.h"
#include "../../services/application/HostMaintenanceCoordinator.h"
#include "../../network/BrickSuiteNetworkManager.h"
#include "../../network/BrickSuiteWebSocketClient.h"
#include "../../network/BrickSuiteWebSocketServer.h"
#include "../../network/BrickSuiteHostIdentity.h"
#include "../../api/brickset/BricksetService.h"
#include "../../api/ApiProviderStatusRegistry.h"
#include "../../settings/ThemeManager.h"
#include "../../settings/UserSettings.h"
#include "../../database/DatabaseSchema.h"
#include "../../services/database/AutomaticBackupPolicy.h"
#include "../../services/database/AutomaticBackupService.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QDir>
#include <QFormLayout>
#include <QFileDialog>
#include <QGroupBox>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPushButton>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

SettingsDialog::SettingsDialog(WorkspaceContext& workspaceContext,
                               WorkspaceApplicationService& workspaceService,
                               BrickSuiteNetworkManager& networkManager,
                               AutomaticBackupService* automaticBackupService,
                               QWidget* parent)
    : QDialog(parent)
    , m_workspaceContext(workspaceContext)
    , m_workspaceService(workspaceService)
    , m_networkManager(networkManager)
    , m_automaticBackupService(automaticBackupService)
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
    buildDatabaseBackupTab();
    buildServerTab();
    buildApisTab();

    m_rebrickableApiClient = new RebrickableApiClient(this);
    m_bricksetService = new BricksetService(this);

    connect(&m_networkManager, &BrickSuiteNetworkManager::statusChanged,
            this, &SettingsDialog::updateNetworkPresentation);
    if (auto* maintenance = m_networkManager.maintenanceCoordinator()) {
        connect(maintenance, &HostMaintenanceCoordinator::stateChanged,
                this, &SettingsDialog::updateNetworkPresentation);
        connect(maintenance, &HostMaintenanceCoordinator::countersChanged,
                this, &SettingsDialog::updateNetworkPresentation);
        connect(maintenance, &HostMaintenanceCoordinator::maintenanceEntryFailed,
                this, [this](const QString& message) {
            QMessageBox::warning(this, tr("Host Maintenance"), message);
        });
    }
    connect(m_networkManager.client(), &BrickSuiteWebSocketClient::trustRequired,
            this, [this](const QString& fingerprint) {
        const auto choice = QMessageBox::question(
            this, tr("Trust BrickSuite Host"),
            tr("Before trusting this Host, verify that this fingerprint exactly matches the "
               "fingerprint shown in BrickSuite Server Settings on the Host computer.\n\n%1\n\n"
               "Trust this certificate?").arg(fingerprint));
        if (choice == QMessageBox::Yes) {
            m_hostFingerprintEdit->setText(fingerprint);
            UserSettings::instance().setBrickSuiteTrustedFingerprint(fingerprint);
            m_networkManager.client()->configure(QUrl(m_hostEndpointEdit->text().trimmed()),
                                                  fingerprint,
                                                  m_hostTokenEdit->text(), false);
            m_networkManager.client()->connectToHost();
        } else {
            m_hostTestButton->setEnabled(true);
        }
    });
    connect(m_networkManager.client(), &BrickSuiteWebSocketClient::testConnectionCompleted,
            this, [this](bool, const QString& message) {
        m_hostConnectionStatusLabel->setText(message);
        m_hostTestButton->setEnabled(true);
    });

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::cancelSettings);

    if (m_automaticBackupService) {
        connect(m_automaticBackupService, &AutomaticBackupService::stateChanged,
                this, &SettingsDialog::updateBackupPresentation);
        connect(m_automaticBackupService, &AutomaticBackupService::backupStarted,
                this, [this](const QString&) {
                    m_backupNowButton->setEnabled(false);
                    updateBackupPresentation();
                });
        connect(m_automaticBackupService, &AutomaticBackupService::backupSucceeded,
                this, [this](const QString&) {
                    updateBackupPresentation();
                    if (m_explicitBackupRunning) {
                        m_explicitBackupRunning = false;
                        QMessageBox::information(this, "Automatic Database Backup",
                                                 "Automatic database backup completed and verified.");
                    }
                });
        connect(m_automaticBackupService, &AutomaticBackupService::backupFailed,
                this, [this](DatabaseManager::BackupFailure, const QString& message) {
                    updateBackupPresentation();
                    if (m_explicitBackupRunning) {
                        m_explicitBackupRunning = false;
                        QMessageBox::warning(this, "Automatic Database Backup",
                                             QString("The automatic database backup failed.\n\n%1")
                                                 .arg(message));
                    }
                });
        connect(m_automaticBackupService, &AutomaticBackupService::retentionWarning,
                this, [this](const QString& message) {
                    updateBackupPresentation();
                    if (m_explicitBackupRunning) {
                        m_explicitBackupRunning = false;
                        QMessageBox::warning(this, "Automatic Backup Retention", message);
                    }
                });
    }

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

    if (!m_workspaceService.status().isAvailable()) {
        m_defaultWorkspaceCombo->addItem("Unavailable while using BrickSuite Host", 0);
        m_defaultWorkspaceCombo->setEnabled(false);
        return;
    }

    m_defaultWorkspaceCombo->addItem("(None)", 0);

    const QList<Workspace> workspaces = m_workspaceService.list();

    for (const Workspace& workspace : workspaces) {
        m_defaultWorkspaceCombo->addItem(workspace.name(), workspace.id());
    }
}

void SettingsDialog::loadSettings()
{
    UserSettings& settings = UserSettings::instance();

    m_originalSharedDataSource = settings.sharedDataSource();
    const int sourceIndex = m_sharedDataSourceCombo->findData(
        static_cast<int>(m_originalSharedDataSource));
    m_sharedDataSourceCombo->setCurrentIndex(sourceIndex >= 0 ? sourceIndex : 0);

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

    m_automaticBackupEnabledCheck->setChecked(settings.automaticBackupEnabled());
    const QString savedRoot = settings.automaticBackupRoot();
    m_backupRootEdit->setText(savedRoot.isEmpty()
                                  ? AutomaticBackupPolicy::initialRootSuggestion()
                                  : savedRoot);
    const int frequencyIndex = m_backupFrequencyCombo->findData(
        settings.automaticBackupFrequencyHours());
    m_backupFrequencyCombo->setCurrentIndex(frequencyIndex >= 0 ? frequencyIndex : 4);
    m_backupRetentionSpin->setValue(settings.automaticBackupRetentionCount());
    updateBackupPresentation();

    m_originalServerEnabled = settings.brickSuiteServerEnabled();
    m_originalServerBindAddress = settings.brickSuiteServerBindAddress();
    m_originalServerPort = settings.brickSuiteServerPort();
    m_serverEnabledCheck->setChecked(m_originalServerEnabled);
    int bindIndex = m_serverBindCombo->findData(m_originalServerBindAddress);
    if (bindIndex < 0) {
        m_serverBindCombo->addItem(m_originalServerBindAddress,
                                   m_originalServerBindAddress);
        bindIndex = m_serverBindCombo->count() - 1;
    }
    m_serverBindCombo->setCurrentIndex(bindIndex);
    m_serverPortSpin->setValue(m_originalServerPort);
    m_hostEndpointEdit->setText(settings.brickSuiteHostEndpoint());
    m_hostFingerprintEdit->setText(settings.brickSuiteTrustedFingerprint());
    QString clientCredentialError;
    m_hostTokenEdit->setText(m_networkManager.clientToken(&clientCredentialError));
    m_hostReconnectCheck->setChecked(settings.brickSuiteReconnectAutomatically());
    updateNetworkPresentation();
}

void SettingsDialog::saveSettings()
{
    UserSettings& settings = UserSettings::instance();

    const QString backupRootText = m_backupRootEdit->text().trimmed();
    const QString backupRoot = backupRootText.isEmpty()
                                   ? QString() : QDir::cleanPath(backupRootText);
    QString backupRootError;
    if (!AutomaticBackupPolicy::validateRoot(backupRoot, &backupRootError)) {
        QMessageBox::warning(this, "Automatic Database Backup", backupRootError);
        m_tabWidget->setCurrentWidget(m_automaticBackupEnabledCheck->parentWidget()->parentWidget());
        m_backupRootEdit->setFocus();
        return;
    }

    const int resultsPerPage = m_resultsPerPageCombo->currentData().toInt();

    const int defaultWorkspaceId = m_defaultWorkspaceCombo->currentData().toInt();

    const auto theme = static_cast<UserSettings::Theme>(m_themeCombo->currentData().toInt());
    const auto sharedDataSource = static_cast<SharedDataSource>(
        m_sharedDataSourceCombo->currentData().toInt());

    if (sharedDataSource == SharedDataSource::BrickSuiteHost
        && m_serverEnabledCheck->isChecked()) {
        QMessageBox::warning(this, tr("BrickSuite Server"),
            tr("A BrickSuite Host client cannot also run BrickSuite Server. Disable the Server "
               "before selecting BrickSuite Host as the Shared Data Source."));
        return;
    }
    const QUrl hostEndpoint(m_hostEndpointEdit->text().trimmed());
    if (hostEndpoint.scheme() != QStringLiteral("wss") || hostEndpoint.host().isEmpty()
        || hostEndpoint.port() < 1 || !hostEndpoint.userInfo().isEmpty()
        || hostEndpoint.hasQuery() || hostEndpoint.hasFragment()) {
        QMessageBox::warning(this, tr("BrickSuite Host"),
            tr("Enter a complete secure endpoint such as wss://host.example:47826. "
               "Credentials, query strings, and fragments are not allowed in the endpoint."));
        return;
    }

    const QString apiKey = m_apiKeyEdit->text().trimmed();
    const QString bricksetApiKey = m_bricksetApiKeyEdit->text().trimmed();

    settings.setResultsPerPage(resultsPerPage);

    if (m_workspaceService.status().isAvailable())
        settings.setDefaultWorkspaceId(defaultWorkspaceId);

    settings.setTheme(theme);
    settings.setSharedDataSource(sharedDataSource);
    settings.setBrickSuiteServerEnabled(m_serverEnabledCheck->isChecked());
    settings.setBrickSuiteServerBindAddress(m_serverBindCombo->currentData().toString());
    settings.setBrickSuiteServerPort(m_serverPortSpin->value());
    settings.setBrickSuiteHostEndpoint(hostEndpoint.toString(QUrl::FullyEncoded));
    settings.setBrickSuiteTrustedFingerprint(m_hostFingerprintEdit->text());
    settings.setBrickSuiteReconnectAutomatically(m_hostReconnectCheck->isChecked());
    QString networkCredentialError;
    if (!m_networkManager.saveClientToken(m_hostTokenEdit->text(), &networkCredentialError)) {
        QMessageBox::critical(this, tr("BrickSuite Host Access Token"),
            tr("BrickSuite could not save the Host access token securely.\n\n%1")
                .arg(networkCredentialError));
        return;
    }

    const bool rebrickableKeyChanged =
        (apiKey != m_originalRebrickableApiKey.trimmed());

    QString credentialError;

    if (!settings.setRebrickableApiKey(apiKey, &credentialError)) {
        QMessageBox::critical(
            this,
            tr("Rebrickable API Key"),
            tr("BrickSuite could not save the Rebrickable API key securely.\n\n"
               "%1\n\n"
               "No plaintext API key was written to application settings.")
                .arg(credentialError));
        return;
    }

    if (rebrickableKeyChanged
        && m_rebrickableConnectionStatus != ApiConnectionStatus::Connected) {
        settings.setRebrickableConnectionPreviouslyVerified(false);
    }

    const bool bricksetKeyChanged =
        (bricksetApiKey != m_originalBricksetApiKey.trimmed());

    credentialError.clear();

    if (!settings.setBricksetApiKey(bricksetApiKey, &credentialError)) {
        QMessageBox::critical(
            this,
            tr("Brickset API Key"),
            tr("BrickSuite could not save the Brickset API key securely.\n\n"
               "%1\n\n"
               "No plaintext API key was written to application settings.")
                .arg(credentialError));
        return;
    }

    if (bricksetKeyChanged
        && m_bricksetConnectionStatus != ApiConnectionStatus::Connected) {
        settings.setBricksetConnectionPreviouslyVerified(false);
    }

    settings.setBricksetDailyGetSetsThreshold(m_bricksetDailyThresholdSpin->value());

    const int rebrickableRequestIntervalMs = m_rebrickableRequestIntervalSpin->value();
    settings.setRebrickableMinimumRequestIntervalMs(rebrickableRequestIntervalMs);
    settings.setAutomaticBackupEnabled(m_automaticBackupEnabledCheck->isChecked());
    settings.setAutomaticBackupRoot(backupRoot);
    settings.setAutomaticBackupFrequencyHours(m_backupFrequencyCombo->currentData().toInt());
    settings.setAutomaticBackupRetentionCount(m_backupRetentionSpin->value());

    if (m_automaticBackupService)
        m_automaticBackupService->reloadPolicy();

    const QString serverBindAddress = m_serverBindCombo->currentData().toString();
    const bool serverConfigurationChanged = m_serverEnabledCheck->isChecked()
        != m_originalServerEnabled
        || serverBindAddress != m_originalServerBindAddress
        || m_serverPortSpin->value() != m_originalServerPort
        || sharedDataSource != m_originalSharedDataSource;
    if (sharedDataSource == SharedDataSource::ThisComputer && serverConfigurationChanged) {
        QString serverError;
        if (!m_networkManager.restartServer(&serverError) && m_serverEnabledCheck->isChecked())
            QMessageBox::warning(this, tr("BrickSuite Server"), serverError);
    }

    if (QApplication* application = qobject_cast<QApplication*>(QApplication::instance())) {
        ThemeManager::applyTheme(*application, theme);
    }

    emit settingsChanged();

    if (sharedDataSource != m_originalSharedDataSource) {
        const QString message = sharedDataSource == SharedDataSource::BrickSuiteHost
            ? tr("This device will continue using its local Rebrickable catalogs and image cache. "
                 "After restart, Workspaces, Storage, Inventory, Builds, and Collection will come "
                 "from the BrickSuite Host. Existing local workshop data will remain on this device "
                 "but will not be shown while Host mode is active.\n\n"
                 "BrickSuite Host connectivity is not available yet. Restart BrickSuite to apply this change.")
            : tr("After restart, BrickSuite will again use this device's local Workspaces, Storage, "
                 "Inventory, Builds, and Collection. Host data will not be copied or merged.\n\n"
                 "Restart BrickSuite to apply this change.");
        QMessageBox::information(this, tr("Shared Data Source Changed"), message);
    }

    accept();
}

void SettingsDialog::buildDatabaseBackupTab()
{
    auto* tab = new QWidget(m_tabWidget);
    auto* layout = new QVBoxLayout(tab);
    auto* group = new QGroupBox("Automatic Database Backup", tab);
    auto* form = new QFormLayout(group);

    m_automaticBackupEnabledCheck = new QCheckBox("Enable automatic database backups", group);
    m_automaticBackupEnabledCheck->setObjectName("automaticBackupEnabledCheck");
    form->addRow(m_automaticBackupEnabledCheck);

    auto* rootRow = new QWidget(group);
    auto* rootLayout = new QHBoxLayout(rootRow);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    m_backupRootEdit = new QLineEdit(rootRow);
    m_backupRootEdit->setObjectName("automaticBackupRootEdit");
    auto* browseButton = new QPushButton("Browse...", rootRow);
    rootLayout->addWidget(m_backupRootEdit, 1);
    rootLayout->addWidget(browseButton);
    form->addRow("Backup root:", rootRow);

    m_currentBackupFolderLabel = new QLabel(group);
    m_currentBackupFolderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_currentBackupFolderLabel->setWordWrap(true);
    form->addRow("Current backup folder:", m_currentBackupFolderLabel);

    m_backupFrequencyCombo = new QComboBox(group);
    m_backupFrequencyCombo->setObjectName("automaticBackupFrequencyCombo");
    for (const int hours : AutomaticBackupPolicy::supportedFrequencyHours()) {
        m_backupFrequencyCombo->addItem(
            AutomaticBackupPolicy::frequencyDisplayText(hours), hours);
    }
    form->addRow("Frequency:", m_backupFrequencyCombo);

    m_backupRetentionSpin = new QSpinBox(group);
    m_backupRetentionSpin->setRange(1, 365);
    m_backupRetentionSpin->setObjectName("automaticBackupRetentionSpin");
    m_backupRetentionSpin->setToolTip(
        "Applies only to automatic backups in the current schema-version folder. "
        "Manual backups and previous schema-version folders are never deleted.");
    form->addRow("Retain last:", m_backupRetentionSpin);

    m_lastBackupLabel = new QLabel(group);
    m_lastBackupLabel->setObjectName("automaticBackupLastSuccessLabel");
    m_lastBackupFileLabel = new QLabel(group);
    m_lastBackupFileLabel->setObjectName("automaticBackupLastFileLabel");
    m_lastBackupFileLabel->setWordWrap(true);
    m_lastBackupFileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_lastBackupFailureLabel = new QLabel(group);
    m_lastBackupFailureLabel->setObjectName("automaticBackupLastFailureLabel");
    m_lastBackupFailureLabel->setWordWrap(true);
    m_nextBackupDueLabel = new QLabel(group);
    m_nextBackupDueLabel->setObjectName("automaticBackupNextDueLabel");
    form->addRow("Last successful backup:", m_lastBackupLabel);
    form->addRow("Last successful file:", m_lastBackupFileLabel);
    form->addRow("Last failure:", m_lastBackupFailureLabel);
    form->addRow("Next backup due:", m_nextBackupDueLabel);

    m_backupNowButton = new QPushButton("Backup Now", group);
    m_backupNowButton->setObjectName("automaticBackupNowButton");
    form->addRow(m_backupNowButton);
    layout->addWidget(group);
    layout->addStretch();
    m_tabWidget->addTab(tab, "Database Backup");

    connect(browseButton, &QPushButton::clicked, this, &SettingsDialog::browseBackupRoot);
    connect(m_backupRootEdit, &QLineEdit::textChanged, this, &SettingsDialog::updateBackupPresentation);
    connect(m_backupFrequencyCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::updateBackupPresentation);
    connect(m_automaticBackupEnabledCheck, &QCheckBox::toggled,
            this, &SettingsDialog::updateBackupPresentation);
    connect(m_backupNowButton, &QPushButton::clicked, this, &SettingsDialog::backupNow);
}

void SettingsDialog::browseBackupRoot()
{
    const QString selected = QFileDialog::getExistingDirectory(
        this, "Select Automatic Backup Root", m_backupRootEdit->text().trimmed());
    if (!selected.isEmpty())
        m_backupRootEdit->setText(QDir::cleanPath(selected));
}

void SettingsDialog::updateBackupPresentation()
{
    if (!m_backupRootEdit) return;
    const QString rootText = m_backupRootEdit->text().trimmed();
    const QString root = rootText.isEmpty() ? QString() : QDir::cleanPath(rootText);
    m_currentBackupFolderLabel->setText(root.isEmpty()
        ? "Not configured"
        : QDir::toNativeSeparators(AutomaticBackupPolicy::versionDirectory(
              root, DatabaseSchema::CurrentSchemaVersion)));
    UserSettings& settings = UserSettings::instance();
    const QDateTime success = settings.automaticBackupLastSuccessfulUtc();
    m_lastBackupLabel->setText(success.isValid()
                                   ? QLocale().toString(success.toLocalTime(), QLocale::ShortFormat)
                                   : "Never");
    m_lastBackupFileLabel->setText(settings.automaticBackupLastSuccessfulPath().isEmpty()
                                       ? "Never"
                                       : QDir::toNativeSeparators(settings.automaticBackupLastSuccessfulPath()));
    const QDateTime failure = settings.automaticBackupLastFailureUtc();
    m_lastBackupFailureLabel->setText(failure.isValid()
        ? QString("%1 — %2").arg(QLocale().toString(failure.toLocalTime(), QLocale::ShortFormat),
                                  settings.automaticBackupLastFailureSummary())
        : "None");
    if (!m_automaticBackupEnabledCheck->isChecked()) {
        m_nextBackupDueLabel->setText("Disabled");
    } else if (!success.isValid()) {
        m_nextBackupDueLabel->setText("Due now");
    } else {
        const QDateTime due = success.addSecs(
            m_backupFrequencyCombo->currentData().toInt() * 60 * 60);
        m_nextBackupDueLabel->setText(due <= QDateTime::currentDateTimeUtc()
                                          ? "Due now"
                                          : QLocale().toString(due.toLocalTime(), QLocale::ShortFormat));
    }
    QString error;
    const bool usable = AutomaticBackupPolicy::validateRoot(root, &error);
    m_backupNowButton->setEnabled(usable && m_automaticBackupService
                                  && !m_automaticBackupService->isRunning());
    m_backupRootEdit->setToolTip(usable ? QString() : error);
}

void SettingsDialog::backupNow()
{
    if (!m_automaticBackupService) return;
    const QString rootText = m_backupRootEdit->text().trimmed();
    const QString root = rootText.isEmpty() ? QString() : QDir::cleanPath(rootText);
    QString error;
    if (!AutomaticBackupPolicy::validateRoot(root, &error)) {
        QMessageBox::warning(this, "Automatic Database Backup", error);
        return;
    }
    m_explicitBackupRunning = true;
    m_backupNowButton->setEnabled(false);
    m_automaticBackupService->requestBackupNow(root, m_backupRetentionSpin->value());
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

    m_sharedDataSourceCombo = new QComboBox(generalGroup);
    m_sharedDataSourceCombo->addItem("This Computer",
                                     static_cast<int>(SharedDataSource::ThisComputer));
    m_sharedDataSourceCombo->addItem("BrickSuite Host",
                                     static_cast<int>(SharedDataSource::BrickSuiteHost));
    m_sharedDataSourceCombo->setToolTip(
        "Select where shared workshop data comes from. Changes take effect after restart.");

    auto* sharedDataDescription = new QLabel(
        "This Computer owns both reference catalogs and workshop data. BrickSuite Host keeps "
        "reference catalogs and images on this device while shared workshop data comes from the Host. "
        "Changing this setting requires a restart.", generalGroup);
    sharedDataDescription->setWordWrap(true);

    generalLayout->addRow("Results per page:", m_resultsPerPageCombo);

    generalLayout->addRow("Default workspace:", m_defaultWorkspaceCombo);

    generalLayout->addRow("Shared Data Source:", m_sharedDataSourceCombo);
    generalLayout->addRow(QString(), sharedDataDescription);

    layout->addWidget(generalGroup);

    layout->addStretch();

    m_tabWidget->addTab(tab, "General");
}

void SettingsDialog::buildServerTab()
{
    auto* tab = new QWidget(m_tabWidget);
    auto* layout = new QVBoxLayout(tab);

    auto* serverGroup = new QGroupBox(tr("BrickSuite Server (This Computer)"), tab);
    auto* serverForm = new QFormLayout(serverGroup);
    m_serverEnabledCheck = new QCheckBox(tr("Enable BrickSuite Server"), serverGroup);
    m_serverBindCombo = new QComboBox(serverGroup);
    m_serverBindCombo->addItem(tr("Loopback only — 127.0.0.1"), QStringLiteral("127.0.0.1"));
    m_serverBindCombo->addItem(tr("Loopback only — ::1"), QStringLiteral("::1"));
    for (const QHostAddress& address : QNetworkInterface::allAddresses()) {
        if (address.isLoopback() || address.protocol() == QAbstractSocket::UnknownNetworkLayerProtocol)
            continue;
        const QString value = address.toString();
        if (m_serverBindCombo->findData(value) < 0)
            m_serverBindCombo->addItem(tr("Interface — %1").arg(value), value);
    }
    m_serverBindCombo->addItem(tr("All IPv4 interfaces (advanced)"), QStringLiteral("0.0.0.0"));
    m_serverBindCombo->addItem(tr("All IPv6 interfaces (advanced)"), QStringLiteral("::"));
    m_serverPortSpin = new QSpinBox(serverGroup);
    m_serverPortSpin->setRange(1024, 65535);
    m_serverStatusLabel = new QLabel(serverGroup);
    m_serverStatusLabel->setWordWrap(true);
    m_serverFingerprintLabel = new QLabel(tr("Not generated"), serverGroup);
    m_serverFingerprintLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_serverFingerprintLabel->setWordWrap(true);
    m_serverTokenButton = new QPushButton(tr("Generate / Rotate Access Token..."), serverGroup);
    auto* regenerateButton = new QPushButton(tr("Regenerate Host Identity..."), serverGroup);
    serverForm->addRow(QString(), m_serverEnabledCheck);
    serverForm->addRow(tr("Bind address:"), m_serverBindCombo);
    serverForm->addRow(tr("Port:"), m_serverPortSpin);
    serverForm->addRow(tr("Status:"), m_serverStatusLabel);
    serverForm->addRow(tr("Certificate fingerprint:"), m_serverFingerprintLabel);
    serverForm->addRow(QString(), m_serverTokenButton);
    serverForm->addRow(QString(), regenerateButton);
    m_maintenanceStateLabel = new QLabel(serverGroup);
    m_maintenanceCountersLabel = new QLabel(serverGroup);
    m_enterMaintenanceButton = new QPushButton(tr("Enter Maintenance..."), serverGroup);
    m_leaveMaintenanceButton = new QPushButton(tr("Exit Maintenance"), serverGroup);
    auto* maintenanceButtons = new QWidget(serverGroup);
    auto* maintenanceButtonsLayout = new QHBoxLayout(maintenanceButtons);
    maintenanceButtonsLayout->setContentsMargins(0, 0, 0, 0);
    maintenanceButtonsLayout->addWidget(m_enterMaintenanceButton);
    maintenanceButtonsLayout->addWidget(m_leaveMaintenanceButton);
    maintenanceButtonsLayout->addStretch();
    serverForm->addRow(tr("Maintenance state:"), m_maintenanceStateLabel);
    serverForm->addRow(tr("Host operations:"), m_maintenanceCountersLabel);
    serverForm->addRow(QString(), maintenanceButtons);
    auto* reachability = new QLabel(
        tr("Listening locally does not confirm that your router or firewall permits remote "
           "connections. BrickSuite does not configure port forwarding or firewall rules."), serverGroup);
    reachability->setWordWrap(true);
    serverForm->addRow(QString(), reachability);
    layout->addWidget(serverGroup);

    auto* clientGroup = new QGroupBox(tr("BrickSuite Host Client"), tab);
    auto* clientForm = new QFormLayout(clientGroup);
    m_hostEndpointEdit = new QLineEdit(clientGroup);
    m_hostEndpointEdit->setPlaceholderText(QStringLiteral("wss://host.example:47826"));
    m_hostFingerprintEdit = new QLineEdit(clientGroup);
    m_hostFingerprintEdit->setPlaceholderText(tr("SHA-256 fingerprint from the Host"));
    m_hostTokenEdit = new QLineEdit(clientGroup);
    m_hostTokenEdit->setEchoMode(QLineEdit::Password);
    m_hostReconnectCheck = new QCheckBox(tr("Reconnect automatically"), clientGroup);
    m_hostConnectionStatusLabel = new QLabel(tr("Not tested"), clientGroup);
    m_hostConnectionStatusLabel->setWordWrap(true);
    m_hostTestButton = new QPushButton(tr("Test Connection"), clientGroup);
    clientForm->addRow(tr("Secure endpoint:"), m_hostEndpointEdit);
    clientForm->addRow(tr("Trusted fingerprint:"), m_hostFingerprintEdit);
    clientForm->addRow(tr("Access token:"), m_hostTokenEdit);
    clientForm->addRow(QString(), m_hostReconnectCheck);
    clientForm->addRow(tr("Connection status:"), m_hostConnectionStatusLabel);
    clientForm->addRow(QString(), m_hostTestButton);
    layout->addWidget(clientGroup);
    layout->addStretch();
    m_tabWidget->addTab(tab, tr("Server"));

    connect(m_serverTokenButton, &QPushButton::clicked,
            this, &SettingsDialog::generateOrRotateServerToken);
    connect(regenerateButton, &QPushButton::clicked,
            this, &SettingsDialog::regenerateHostIdentity);
    connect(m_hostTestButton, &QPushButton::clicked,
            this, &SettingsDialog::testBrickSuiteHostConnection);
    connect(m_enterMaintenanceButton, &QPushButton::clicked,
            this, &SettingsDialog::enterHostMaintenance);
    connect(m_leaveMaintenanceButton, &QPushButton::clicked,
            this, &SettingsDialog::leaveHostMaintenance);
    connect(m_sharedDataSourceCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::updateNetworkPresentation);
    connect(m_serverEnabledCheck, &QCheckBox::toggled,
            this, &SettingsDialog::updateNetworkPresentation);
}

void SettingsDialog::updateNetworkPresentation()
{
    if (!m_serverEnabledCheck) return;
    const auto source = static_cast<SharedDataSource>(m_sharedDataSourceCombo->currentData().toInt());
    const bool local = source == SharedDataSource::ThisComputer;
    // Keep an already-enabled checkbox interactive so the user can resolve an
    // invalid Host-client + Server combination by turning the Server off.
    m_serverEnabledCheck->setEnabled(local || m_serverEnabledCheck->isChecked());
    m_serverBindCombo->setEnabled(local);
    m_serverPortSpin->setEnabled(local);
    m_serverTokenButton->setEnabled(local);
    m_serverStatusLabel->setText(local ? m_networkManager.serverStatusText()
                                      : tr("Unavailable in BrickSuite Host client mode."));
    const QString fingerprint = m_networkManager.server()->fingerprint();
    m_serverFingerprintLabel->setText(fingerprint.isEmpty() ? tr("Not generated") : fingerprint);
    m_hostEndpointEdit->setEnabled(!local);
    m_hostFingerprintEdit->setEnabled(!local);
    m_hostTokenEdit->setEnabled(!local);
    m_hostReconnectCheck->setEnabled(!local);
    m_hostTestButton->setEnabled(!local);
    if (!local) m_hostConnectionStatusLabel->setText(m_networkManager.connectionStatus().message);
    if (auto* maintenance = m_networkManager.maintenanceCoordinator()) {
        m_maintenanceStateLabel->setText(maintenance->stateText());
        m_maintenanceCountersLabel->setText(
            tr("Reads: %1 queued, %2 active; Writes: %3 queued, %4 active")
                .arg(maintenance->queuedReads()).arg(maintenance->activeReads())
                .arg(maintenance->queuedWrites()).arg(maintenance->activeWrites()));
        const bool listening = local && m_networkManager.server()->isListening();
        m_enterMaintenanceButton->setEnabled(listening
            && maintenance->state() == HostMaintenanceCoordinator::State::Normal);
        m_leaveMaintenanceButton->setEnabled(listening
            && maintenance->state() == HostMaintenanceCoordinator::State::Maintenance);
    }
}

void SettingsDialog::enterHostMaintenance()
{
    if (QMessageBox::question(this, tr("Enter Host Maintenance"),
        tr("New Host-backed Remote operations will be temporarily rejected while admitted "
           "operations finish. Host-local operational writes will be disabled. Client-local "
           "catalog browsing remains available. Continue?"),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    if (auto* maintenance = m_networkManager.maintenanceCoordinator())
        maintenance->requestEnterMaintenance();
}

void SettingsDialog::leaveHostMaintenance()
{
    if (auto* maintenance = m_networkManager.maintenanceCoordinator())
        maintenance->leaveMaintenance();
}

void SettingsDialog::generateOrRotateServerToken()
{
    if (QMessageBox::warning(this, tr("Generate / Rotate Access Token"),
        tr("Generating a new token invalidates the token used by existing clients. Continue?"),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    QString error;
    const QString token = m_networkManager.generateOrRotateHostToken(&error);
    if (token.isEmpty()) {
        QMessageBox::critical(this, tr("BrickSuite Server"), error);
        return;
    }
    QMessageBox box(QMessageBox::Information, tr("BrickSuite Server Access Token"),
        tr("Copy this token now and store it on each trusted Client. It will not remain visible."),
        QMessageBox::Ok, this);
    box.setDetailedText(token);
    box.exec();
    if (m_networkManager.server()->isListening()) {
        QString restartError;
        if (!m_networkManager.restartServer(&restartError))
            QMessageBox::warning(this, tr("BrickSuite Server"), restartError);
    }
    updateNetworkPresentation();
}

void SettingsDialog::regenerateHostIdentity()
{
    if (QMessageBox::warning(this, tr("Regenerate Host Identity"),
        tr("Previously paired clients will reject this Host until they explicitly trust the new "
           "certificate fingerprint. Continue?"), QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    const auto identity = BrickSuiteHostIdentity::regenerate();
    if (!identity.success) {
        QMessageBox::critical(this, tr("BrickSuite Server"), identity.error);
        return;
    }
    m_serverFingerprintLabel->setText(identity.fingerprint);
    QString error;
    if (m_networkManager.server()->isListening() && !m_networkManager.restartServer(&error))
        QMessageBox::warning(this, tr("BrickSuite Server"), error);
}

void SettingsDialog::testBrickSuiteHostConnection()
{
    m_hostTestButton->setEnabled(false);
    m_hostConnectionStatusLabel->setText(tr("Testing secure connection..."));
    m_networkManager.client()->disconnectFromHost();
    m_networkManager.client()->configure(QUrl(m_hostEndpointEdit->text().trimmed()),
                                          m_hostFingerprintEdit->text().trimmed(),
                                          m_hostTokenEdit->text(), false);
    m_networkManager.client()->connectToHost();
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

    ApiProviderStatusRegistry::instance().setStatus(ApiProvider::Rebrickable, status);

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

    ApiProviderStatusRegistry::instance().setStatus(ApiProvider::Brickset, status);

    if (m_bricksetStatusLabel)
        m_bricksetStatusLabel->setText(apiConnectionStatusText(status));
}

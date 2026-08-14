#include "SettingsDialog.h"

#include "../../app/WorkspaceContext.h"
#include "../../models/Workspace.h"
#include "../../repositories/WorkspaceRepository.h"
#include "../../services/RebrickableApiClient.h"
#include "../../settings/ThemeManager.h"
#include "../../settings/UserSettings.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

SettingsDialog::SettingsDialog(WorkspaceContext& workspaceContext, QWidget* parent)
    : QDialog(parent)
    , m_workspaceContext(workspaceContext)
{
    setWindowTitle("BrickSuite Settings");

    resize(600, 420);

    auto* mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);

    mainLayout->addWidget(m_tabWidget);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    mainLayout->addWidget(m_buttonBox);

    buildGeneralTab();
    buildAppearanceTab();
    buildRebrickableTab();

    m_rebrickableApiClient = new RebrickableApiClient(this);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::connectionTestFinished,
            this,
            [this](const RebrickableApiClient::ConnectionResult& result) {
                m_testConnectionButton->setEnabled(true);

                if (result.success) {
                    QMessageBox::information(this, "Rebrickable", result.message);
                } else {
                    QMessageBox::warning(this, "Rebrickable", result.message);
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

    const int themeIndex = m_themeCombo->findData(static_cast<int>(settings.theme()));

    if (themeIndex >= 0) {
        m_themeCombo->setCurrentIndex(themeIndex);
    }

    m_apiKeyEdit->setText(settings.rebrickableApiKey());
    m_rebrickableRequestIntervalSpin->setValue(settings.rebrickableMinimumRequestIntervalMs());
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

    settings.setRebrickableApiKey(apiKey);

    const int rebrickableRequestIntervalMs = m_rebrickableRequestIntervalSpin->value();
    settings.setRebrickableMinimumRequestIntervalMs(rebrickableRequestIntervalMs);

    if (QApplication* application = qobject_cast<QApplication*>(QApplication::instance())) {
        ThemeManager::applyTheme(*application, theme);
    }

    emit settingsChanged();

    accept();
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

    appearanceLayout->addRow("Theme:", m_themeCombo);

    layout->addWidget(appearanceGroup);

    layout->addStretch();

    m_tabWidget->addTab(tab, "Appearance");
}

void SettingsDialog::buildRebrickableTab()
{
    auto* tab = new QWidget(m_tabWidget);

    auto* layout = new QVBoxLayout(tab);

    auto* apiGroup = new QGroupBox("Rebrickable API", tab);

    auto* apiLayout = new QFormLayout(apiGroup);

    m_apiKeyEdit = new QLineEdit(apiGroup);

    m_apiKeyEdit->setEchoMode(QLineEdit::Password);

    m_apiKeyEdit->setPlaceholderText("Enter Rebrickable API key");

    m_showApiKeyCheck = new QCheckBox("Show API key", apiGroup);

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

    m_tabWidget->addTab(tab, "Rebrickable");

    connect(m_showApiKeyCheck, &QCheckBox::toggled, this, &SettingsDialog::showApiKeyToggled);
}

void SettingsDialog::showApiKeyToggled(bool checked)
{
    m_apiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}

void SettingsDialog::testRebrickableConnection()
{
    const QString apiKey = m_apiKeyEdit->text().trimmed();

    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, "Rebrickable", "Enter your Rebrickable API key first.");

        return;
    }

    m_testConnectionButton->setEnabled(false);

    m_rebrickableApiClient->testConnection(apiKey);
}
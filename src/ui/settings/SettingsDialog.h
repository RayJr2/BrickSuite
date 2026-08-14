#pragma once

#include <QDialog>

class WorkspaceContext;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QTabWidget;
class QSpinBox;

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
    void showApiKeyToggled(bool checked);
    void testRebrickableConnection();

private:
    void buildGeneralTab();
    void buildAppearanceTab();
    void buildRebrickableTab();

    void loadSettings();
    void loadWorkspaces();

    WorkspaceContext& m_workspaceContext;

    QTabWidget* m_tabWidget = nullptr;

    // General
    QComboBox* m_resultsPerPageCombo = nullptr;
    QComboBox* m_defaultWorkspaceCombo = nullptr;

    // Appearance
    QComboBox* m_themeCombo = nullptr;

    // Rebrickable
    QLineEdit* m_apiKeyEdit = nullptr;
    QCheckBox* m_showApiKeyCheck = nullptr;
    QPushButton* m_testConnectionButton = nullptr;
    QSpinBox* m_rebrickableRequestIntervalSpin = nullptr;

    RebrickableApiClient* m_rebrickableApiClient = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;
};
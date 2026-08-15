#pragma once

#include <QWidget>

class WorkspaceContext;

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextEdit;
class QCheckBox;
class QSpinBox;
class QGroupBox;
class QWidget;
class QSplitter;

class BuildsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BuildsWidget(
        WorkspaceContext& workspaceContext,
        QWidget* parent = nullptr);

    void refresh();
    void selectBuild(int buildId);

private slots:
    void workspaceChanged(
        int workspaceId);

    void addBuild();
    void buildSelectionChanged();
    void addRequirement();

private:
    void loadBuilds();
    void updateUiState();

    void loadRequirements();
    void loadColors();
    void updateRequirementUiState();
    void exportPullList();
    void importPullList();

    WorkspaceContext& m_workspaceContext;

    QComboBox* m_typeCombo = nullptr;
    QLineEdit* m_setNumberEdit = nullptr;
    QComboBox* m_inventoryModeCombo = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_statusCombo = nullptr;
    QTextEdit* m_notesEdit = nullptr;

    QPushButton* m_addButton = nullptr;

    QTableWidget* m_buildsTable = nullptr;
    QSplitter* m_buildsRequirementsSplitter = nullptr;
    QLabel* m_statusLabel = nullptr;

    int m_selectedBuildId = 0;

    QLabel* m_requirementsLabel = nullptr;

    QLineEdit* m_partNumberEdit = nullptr;
    QComboBox* m_colorCombo = nullptr;
    QSpinBox* m_quantitySpin = nullptr;
    QCheckBox* m_spareCheck = nullptr;

    QPushButton* m_addRequirementButton = nullptr;

    QPushButton* m_loadSetFromRebrickableButton = nullptr;
    QPushButton* m_exportPullListButton = nullptr;
    QPushButton* m_importPullListButton = nullptr;

    QTableWidget* m_requirementsTable = nullptr;

    QGroupBox* m_newBuildGroup = nullptr;
    QWidget* m_newBuildContent = nullptr;
};
#pragma once

#include <QDialog>

class WorkspaceContext;
class QLineEdit;
class QComboBox;
class QPushButton;
class QDialogButtonBox;

class ImportInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportInventoryDialog(WorkspaceContext& workspaceContext, QWidget* parent = nullptr);

private slots:
    void browseForFile();
    void importFile();

private:
    void loadStorageLocations();
    void suggestStorageFromFileName(const QString& filePath);
    static QString normalizedStorageKey(const QString& text);

    WorkspaceContext& m_workspaceContext;

    QLineEdit* m_fileEdit = nullptr;
    QPushButton* m_browseButton = nullptr;

    QComboBox* m_storageCombo = nullptr;
    QComboBox* m_conditionCombo = nullptr;
    QComboBox* m_ownershipCombo = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;
};
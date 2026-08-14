#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QComboBox;
class QTextEdit;
class QPushButton;

class EditBuildDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditBuildDialog(int buildId, QWidget* parent = nullptr);

private:
    bool loadBuild();
    void saveBuild();

    int m_buildId = 0;

    QLabel* m_typeLabel = nullptr;
    QLabel* m_setNumberLabel = nullptr;
    QLabel* m_inventoryModeLabel = nullptr;

    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_statusCombo = nullptr;
    QTextEdit* m_notesEdit = nullptr;

    QPushButton* m_saveButton = nullptr;
};
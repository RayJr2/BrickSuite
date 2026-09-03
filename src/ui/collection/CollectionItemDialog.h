#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QTextEdit;

class CollectionItemDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CollectionItemDialog(int collectionItemId, QWidget* parent = nullptr);

signals:
    void itemChanged();

private:
    void save();
    int m_itemId = 0;
    int m_allowPartsSource = 0;
    QComboBox* m_stateCombo = nullptr;
    QComboBox* m_locationCombo = nullptr;
    QLineEdit* m_nicknameEdit = nullptr;
    QTextEdit* m_notesEdit = nullptr;
};

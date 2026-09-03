#pragma once

#include "../../models/CollectionItem.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QTextEdit;

class CatalogCollectionDialog : public QDialog
{
    Q_OBJECT
public:
    CatalogCollectionDialog(int workspaceId, CollectionItemType type, int catalogId,
                            const QString& reference, const QString& name,
                            QWidget* parent = nullptr, int sourceBuildId = 0);

    int createdCollectionItemId() const;

private:
    void createItem();

    int m_workspaceId = 0;
    CollectionItemType m_type = CollectionItemType::Invalid;
    int m_catalogId = 0;
    int m_createdItemId = 0;
    int m_sourceBuildId = 0;
    QComboBox* m_stateCombo = nullptr;
    QComboBox* m_conditionCombo = nullptr;
    QComboBox* m_completenessCombo = nullptr;
    QComboBox* m_locationCombo = nullptr;
    QLineEdit* m_nicknameEdit = nullptr;
    QTextEdit* m_notesEdit = nullptr;
};

/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */

#pragma once

#include <QDialog>

class WorkspaceContext;
class QLabel;
class QLineEdit;
class QSpinBox;
class QDialogButtonBox;
class QCompleter;
class QStandardItemModel;
class QTimer;
class QModelIndex;

class CorrectInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CorrectInventoryDialog(int inventoryRecordId,
                                    WorkspaceContext& workspaceContext,
                                    QWidget* parent = nullptr);

private slots:
    void updatePartSearch();
    void applySelectedPart(const QModelIndex& index);
    void resolveEnteredPart();
    void saveCorrection();

private:
    bool loadInventoryRecord();
    void setResolvedPart(int partId, const QString& displayText);
    void updateSaveButtonState();

    int m_inventoryRecordId = 0;
    int m_originalPartId = 0;
    int m_replacementPartId = 0;
    int m_currentQuantity = 0;

    WorkspaceContext& m_workspaceContext;

    QLabel* m_currentPartLabel = nullptr;
    QLabel* m_contextLabel = nullptr;
    QLabel* m_resolvedLabel = nullptr;
    QLineEdit* m_partSearchEdit = nullptr;
    QSpinBox* m_quantitySpin = nullptr;
    QLineEdit* m_notesEdit = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
    QCompleter* m_completer = nullptr;
    QStandardItemModel* m_searchModel = nullptr;
    QTimer* m_searchTimer = nullptr;
};

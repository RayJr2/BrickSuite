/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QDialog>

class WorkspaceContext;
class SessionStorageSelectionService;
class QLabel;
class QComboBox;
class QSpinBox;
class QDialogButtonBox;

class MoveInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MoveInventoryDialog(int inventoryRecordId,
                                 WorkspaceContext& workspaceContext,
                                 SessionStorageSelectionService& sessionStorageSelectionService,
                                 QWidget* parent = nullptr);

private slots:
    void moveInventory();

private:
    bool loadInventoryRecord();
    void loadStorageLocations();

    int m_inventoryRecordId = 0;
    int m_sourceStorageLocationId = 0;
    int m_availableQuantity = 0;

    WorkspaceContext& m_workspaceContext;
    SessionStorageSelectionService& m_sessionStorageSelectionService;

    QLabel* m_partLabel = nullptr;
    QLabel* m_currentStorageLabel = nullptr;
    QLabel* m_availableLabel = nullptr;

    QComboBox* m_destinationCombo = nullptr;
    QSpinBox* m_quantitySpin = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;
};

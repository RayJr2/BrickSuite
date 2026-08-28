/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */

#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QSpinBox;
class QDialogButtonBox;

class RemoveInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RemoveInventoryDialog(int inventoryRecordId, QWidget* parent = nullptr);

private slots:
    void removeEntry();

private:
    bool loadInventoryRecord();

    int m_inventoryRecordId = 0;
    QLabel* m_partLabel = nullptr;
    QLabel* m_contextLabel = nullptr;
    QSpinBox* m_quantitySpin = nullptr;
    QLineEdit* m_notesEdit = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
};

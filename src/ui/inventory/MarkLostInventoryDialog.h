#pragma once

#include <QDialog>

class QLabel;
class QSpinBox;
class QTextEdit;
class QDialogButtonBox;

class MarkLostInventoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MarkLostInventoryDialog(int inventoryRecordId, QWidget* parent = nullptr);

private slots:
    void markLost();

private:
    bool loadInventory();

    int m_inventoryRecordId = 0;

    int m_currentQuantity = 0;

    QLabel* m_partLabel = nullptr;
    QLabel* m_colorLabel = nullptr;
    QLabel* m_locationLabel = nullptr;
    QLabel* m_currentQuantityLabel = nullptr;

    QSpinBox* m_quantityLostSpin = nullptr;

    QTextEdit* m_notesEdit = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;
};

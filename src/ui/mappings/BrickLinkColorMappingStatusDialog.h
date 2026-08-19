/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QTableWidget;

class BrickLinkColorMappingStatusDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BrickLinkColorMappingStatusDialog(QWidget* parent = nullptr);

private:
    void reload();
    void applyFilter();
    void updateSummary();

    QComboBox* m_filterCombo = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_table = nullptr;
};

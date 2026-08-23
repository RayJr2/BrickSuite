/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

class PartResolverTestDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PartResolverTestDialog(QWidget* parent = nullptr);

private:
    void resolvePart();

    QLineEdit* m_partNumberEdit = nullptr;
    QPushButton* m_resolveButton = nullptr;

    QLabel* m_statusLabel = nullptr;
    QLabel* m_resolvedPartLabel = nullptr;
    QLabel* m_partNameLabel = nullptr;
    QLabel* m_aliasLabel = nullptr;
    QLabel* m_messageLabel = nullptr;

    QTableWidget* m_candidatesTable = nullptr;
};

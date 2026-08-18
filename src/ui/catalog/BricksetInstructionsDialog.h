/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../../api/brickset/BricksetService.h"

#include <QDialog>

class QTableWidget;

class BricksetInstructionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BricksetInstructionsDialog(
        const QString& setNumber,
        const QList<BricksetService::Instruction>& instructions,
        QWidget* parent = nullptr);

private:
    static bool isCoreInstruction(const BricksetService::Instruction& instruction);
    void populateTable(const QList<BricksetService::Instruction>& instructions);

    QString m_setNumber;
    QTableWidget* m_table = nullptr;
};

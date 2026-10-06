/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
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

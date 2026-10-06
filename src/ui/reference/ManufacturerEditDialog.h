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

#include "../../models/Manufacturer.h"
#include <QDialog>

class QCheckBox;
class QLineEdit;
class QTextEdit;

class ManufacturerEditDialog : public QDialog
{
public:
    explicit ManufacturerEditDialog(const Manufacturer* manufacturer = nullptr,
                                    QWidget* parent = nullptr);
    Manufacturer manufacturer() const;

private:
    Manufacturer m_original;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_code = nullptr;
    QLineEdit* m_website = nullptr;
    QCheckBox* m_elementIds = nullptr;
    QTextEdit* m_notes = nullptr;
};

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

#include <QDialog>

class QPushButton;
class QTableWidget;

class ReferenceDataDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ReferenceDataDialog(QWidget* parent = nullptr);

signals:
    void manufacturersChanged();

private:
    int selectedManufacturerId() const;
    void reload();
    void addManufacturer();
    void editManufacturer();
    void changeActiveState(bool active);
    void updateActions();

    QTableWidget* m_table = nullptr;
    QPushButton* m_edit = nullptr;
    QPushButton* m_activate = nullptr;
    QPushButton* m_deactivate = nullptr;
};

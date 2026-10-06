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

class QLabel;
class QLineEdit;
class QPushButton;

class BrickLinkPartMappingTestDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BrickLinkPartMappingTestDialog(QWidget* parent = nullptr);

private:
    void loadPart();
    void refreshResolution();
    void saveMappedOverride();
    void markUnknown();
    void markUnsupported();
    void clearMapping();

    int m_partId = 0;

    QLineEdit* m_partNumberEdit = nullptr;
    QLineEdit* m_overrideEdit = nullptr;

    QLabel* m_partNameLabel = nullptr;
    QLabel* m_itemIdLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_canExportLabel = nullptr;
    QLabel* m_messageLabel = nullptr;

    QPushButton* m_loadButton = nullptr;
    QPushButton* m_saveOverrideButton = nullptr;
    QPushButton* m_unknownButton = nullptr;
    QPushButton* m_unsupportedButton = nullptr;
    QPushButton* m_clearButton = nullptr;
};

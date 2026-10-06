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
class QPlainTextEdit;

class BrickLinkWantedListResultDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BrickLinkWantedListResultDialog(
        const QString& xml,
        int itemRows,
        int totalPieces,
        const QString& buildNumber,
        const QString& buildName,
        QWidget* parent = nullptr);

private:
    QString defaultFileName() const;
    void copyXml();
    void saveXml();
    void openBrickLinkUpload();

    QString m_xml;
    QString m_buildNumber;
    QString m_buildName;

    QLabel* m_statusLabel = nullptr;
    QPlainTextEdit* m_xmlEdit = nullptr;
};

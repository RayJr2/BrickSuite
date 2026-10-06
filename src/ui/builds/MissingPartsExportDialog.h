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

#include "../../models/export/MissingPartsExportTypes.h"

#include <QDialog>

#include <functional>

class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;
class QComboBox;
class QWidget;

class MissingPartsExportDialog : public QDialog
{
    Q_OBJECT

public:
    using PartOverrideResolver = std::function<void(
        const QString&, int, QObject*,
        std::function<void(const PickABrickPartResolution&)>)>;

    explicit MissingPartsExportDialog(QList<MissingPartsExportRow> rows,
                                      QString defaultFileName,
                                      QWidget* parent = nullptr,
                                      PartOverrideResolver partOverrideResolver = {});

private:
    void loadConfiguration();
    void applyConfiguration(const MissingPartsExportConfiguration& configuration);
    MissingPartsExportConfiguration configuration() const;
    void persistConfiguration();
    void updatePreview();
    void updateGeneralPreview();
    void updatePickABrickPreview();
    void updatePickABrickSummary();
    void resolvePartOverride(int rowIndex, const QString& partNumber);
    QString pickABrickRowStatus(const PickABrickExportSourceRow& row) const;
    void moveCurrentField(int offset);
    void exportCsv();
    bool pickABrickSelected() const;

    QList<MissingPartsExportRow> m_rows;
    QList<PickABrickExportSourceRow> m_pickABrickRows;
    QString m_defaultFileName;
    PartOverrideResolver m_partOverrideResolver;
    QComboBox* m_preset = nullptr;
    QWidget* m_generalControls = nullptr;
    QLabel* m_description = nullptr;
    QListWidget* m_fields = nullptr;
    QPushButton* m_moveUp = nullptr;
    QPushButton* m_moveDown = nullptr;
    QTableWidget* m_preview = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_export = nullptr;
};
